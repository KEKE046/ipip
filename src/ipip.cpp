#include "ipip.h"
#include <imgui.h>
#include <implot.h>
#include <vector>
#include <cmath>
#include <memory>
#include <map>
#include <string>
#include <json/json.h>
#include <cassert>
#include <ctime>
#include <iterator>
#include <iostream>
#include <GLFW/glfw3.h>

#include <implot_internal.h>
namespace ImPlot {
}

namespace ipip {

    struct Options {
        float history = 5;
        int colormap = 5;
        bool lock_x = true;
    } option;

    struct Events{
        bool colormap_changed = false;
        bool history_changed = false;
    } event;

    struct Stream {
        float span{60};
        int width{1};
        std::string name{};
        std::vector<double> data;
        std::vector<double> tickmod;
        std::vector<double> buffer;

        Stream(std::string name): name{name}{}

        void updateBuffer(double time) {
            span = option.history;
            if (!tickmod.empty() && fmod(time, span) < tickmod.back()) {
                tickmod.resize(0);
                data.resize(0);
            }
            tickmod.push_back(fmod(time, span));
        }

        void plotLine() {
            ImPlot::PlotLine(name.c_str(), tickmod.data(), data.data(), tickmod.size(), 0, sizeof(float));
        }

        double ymin{0}, ymax{0};

        void plotHeat(float &scale_min, float &scale_max) {
            if(data.size() > buffer.size()) {
                buffer.resize(data.size());
            }
            int n = data.size() / width;
            for(int i = 0; i < n; i++) {
                for(int j = 0; j < width; j++) {
                    buffer[j * n + i] = data[i * width + j];
                    scale_min = std::min(scale_min, float(data[i * width + j]));
                    scale_max = std::max(scale_max, float(data[i * width + j]));
                }
            }
            ImPlot::PlotHeatmap(name.c_str(), buffer.data(), width, data.size() / width, scale_min, scale_max,
                "", ImPlotPoint(tickmod.size() ? tickmod.front() : 0, ymin), ImPlotPoint(tickmod.size() ? tickmod.back() : 0, ymax==ymin ? width : ymax));
        }

        void feed(double time, const std::vector<double> & newdata, double _ymin=0, double _ymax=0) {
            updateBuffer(time);
            width = newdata.size();
            std::copy(newdata.begin(), newdata.end(), std::back_inserter(data));
            ymin = _ymin; ymax = _ymax;
        }

        void feed(Json::Value value) {
            assert(value.isMember("name")  && value["name"].isString());
            assert(value.isMember("time")  && value["time"].isNumeric());
            assert(value.isMember("value") && (value["value"].isArray() || value["value"].isNumeric()));
            double time = value["time"].asDouble();
            double _ymin = ymin, _ymax = ymax;
            Json::Value & newdata = value["value"];
            std::vector<double> _newdata;
            if(newdata.isArray()) {
                int size = newdata.size();
                for(int i = 0; i < size; i++) {
                    _newdata.push_back(newdata[i].asDouble());
                }
                if(value.isMember("ymin")) {
                    assert(value["ymin"].isNumeric());
                    _ymin = value["ymin"].asDouble();
                }
                if(value.isMember("ymax")) {
                    assert(value["ymax"].isNumeric());
                    _ymax = value["ymax"].asDouble();
                }
            }
            else if(newdata.isNumeric()) {
                _newdata.push_back(newdata.asDouble());
            }
            feed(time, _newdata, _ymin, _ymax);
        }
    };

    struct Subplot {
        std::string name;
        float scale[2]{0,0};
        std::vector<Stream> streams;
        Subplot(std::string name): name{name}{};
        Stream & findStream(std::string name) {
            for(auto & stream: streams)
                if(stream.name == name)
                    return stream;
            streams.emplace_back(name);
            return streams.back();
        }
    };

    std::vector<Subplot> figure;
    void showSettings() {
        ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(600, 750), ImGuiCond_FirstUseEver);
        ImGui::Begin("Setting");
        ImGui::Text("History: "); ImGui::SameLine();
        event.history_changed  = ImGui::SliderFloat("##History", &option.history, 1, 120);
        ImGui::Text("ColorMap:"); ImGui::SameLine();
        event.colormap_changed = ImPlot::ColormapButton(ImPlot::GetColormapName(option.colormap),ImVec2(225,0),option.colormap);
        if (event.colormap_changed) {
            option.colormap = (option.colormap + 1) % ImPlot::GetColormapCount();
        }
        ImGui::Text("LockX:   "); ImGui::SameLine();
        ImGui::Checkbox("##LockX", &option.lock_x);
        ImGui::End();
    }

    void showFigure() {
        for(auto & subp: figure) {
            ImGui::Begin(subp.name.c_str());
            std::string plotName = "##" + subp.name + "##PLOT";
            ImPlot::SetNextPlotLimitsX(0, option.history, option.lock_x ? ImGuiCond_Always : NULL);
            ImPlot::SetNextPlotLimitsY(0,1);
            if(event.colormap_changed) {
                ImPlot::BustColorCache(plotName.c_str());
            }
            bool need_vlim = false;
            for(auto & stream: subp.streams) {
                if(stream.width > 1) {
                    need_vlim = true;
                }
            }
            if(need_vlim) {
                std::string name = "Min / Max##" + subp.name + "##VLIM";
                ImGui::InputFloat2(name.c_str(), subp.scale);
            }
            ImPlot::BeginPlot(plotName.c_str(), NULL, NULL, ImVec2(-1,-1));
            ImPlot::SetLegendLocation(ImPlotLocation_East, ImPlotOrientation_Vertical, true);
            for(auto & stream: subp.streams) {
                if(stream.width > 1) {
                    ImPlot::PushColormap(option.colormap);
                    stream.plotHeat(subp.scale[0], subp.scale[1]);
                    ImPlot::PopColormap();
                }
                else {
                    stream.plotLine();
                }
            }
            ImPlot::EndPlot();
            ImGui::End();
        }
    }

    Subplot & findSubplot(std::string name) {
        for(auto & subp: figure)
            if(subp.name == name)
                return subp;
        figure.emplace_back(name);
        return figure.back();
    }

    void feedData(Json::Value data) {
        for(std::string figName: data.getMemberNames()) {
            auto & subp = findSubplot(figName);
            for(std::string streamName: data[figName].getMemberNames()) {
                data[figName][streamName]["name"] = streamName;
                subp.findStream(streamName).feed(data[figName][streamName]);
            }
        }
    }

    Json::Value createData() {
        float tm = glfwGetTime();
        Json::Value stream1;
        stream1["time"] = tm;
        stream1["value"] = sin(tm);
        Json::Value stream2;
        stream2["time"] = tm;
        stream2["value"] = cos(tm);
        Json::Value stream3;
        stream3["time"] = tm;
        stream3["value"] = tan(tm);
        Json::Value stream4;
        stream4["time"] = tm;
        stream4["value"] = Json::Value(Json::arrayValue);
        stream4["value"].append(sin(tm));
        stream4["value"].append(cos(tm));
        Json::Value fig1;
        fig1["sin"] = stream1;
        fig1["cos"] = stream2;
        Json::Value fig2;
        fig2["tan"] = stream3;
        fig2["heat"] = stream4;
        Json::Value root;
        root["fig1"] = fig1;
        root["fig2"] = fig2;
        return root;
    }

    void updateWindow() {
        showSettings();
        feedData(createData());
        showFigure();
    }

} // namespace ipip
