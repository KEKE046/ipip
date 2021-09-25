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
#include "help.h"
#include "server.h"

namespace ipip {

    struct Options {
        float history = 5;
        int colormap = 5;
        bool lock_x = true;
    } option;

    struct Events{
        bool colormap_changed = false;
        bool history_changed = false;
        bool tile_window = false;
    } event;

    struct Stream {
        float span{60};
        int width{1};
        double vmx{-1e100}, vmn{1e100};
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
            ImPlot::PlotLine(name.c_str(), tickmod.data(), data.data(), tickmod.size(), 0, sizeof(double));
        }

        void plotHeat(float &scale_min, float &scale_max) {
            if(data.size() > buffer.size()) {
                buffer.resize(data.size());
            }
            int n = data.size() / width;
            for(int i = 0; i < n; i++) {
                for(int j = 0; j < width; j++) {
                    buffer[j * n + i] = data[i * width + j];
                }
            }
            ImPlot::PlotHeatmap(name.c_str(), buffer.data(), width, data.size() / width, vmn, vmx,
                "", ImPlotPoint(tickmod.size() ? tickmod.front() : 0, 0), ImPlotPoint(tickmod.size() ? tickmod.back() : 0, 1));
        }

        void feed(double time, const std::vector<double> & value) {
            updateBuffer(time);
            width = value.size();
            for(auto v: value) {
                vmx = std::max(vmx, v);
                vmn = std::min(vmn, v);
            }
            std::copy(value.begin(), value.end(), std::back_inserter(data));
        }

        void feed(double time, Json::Value value) {
            ipipAssert(value.isArray() || value.isNumeric(), "Invalid data", value);
            std::vector<double> _value;
            if(value.isArray()) {
                for(int i = 0; i < value.size(); i++) {
                    _value.push_back(value[i].asDouble());
                }
            }
            else if(value.isNumeric()) {
                _value.push_back(value.asDouble());
            }
            feed(time, _value);
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
        static bool firstRun = true;
        static Options lastoption = option;
        if(firstRun) {
            if(FILE * f = fopen("ipip.dat", "r")) {
                fread(&option, sizeof(option), 1, f);
                fclose(f);
            }
        }
        firstRun = false;
        ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(350, 150), ImGuiCond_FirstUseEver);
        ImGui::Begin("Setting", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("History: "); ImGui::SameLine();
        event.history_changed  = ImGui::SliderFloat("##History", &option.history, 1, 120);
        ImGui::Text("ColorMap:"); ImGui::SameLine();
        event.colormap_changed = ImPlot::ColormapButton(ImPlot::GetColormapName(option.colormap),ImVec2(225,0),option.colormap);
        if (event.colormap_changed) {
            option.colormap = (option.colormap + 1) % ImPlot::GetColormapCount();
        }
        ImGui::Text("Tile:    "); ImGui::SameLine();
        event.tile_window = ImGui::Button("do##SettingTile");
        ImGui::Text("Lock X:  "); ImGui::SameLine();
        ImGui::Checkbox("##LockX", &option.lock_x);
        ImGui::Text("Clear:   "); ImGui::SameLine();
        if(ImGui::Button("do##SettingClear")) figure.clear();
        ImGui::End();
        if(memcmp(&option, &lastoption, sizeof(Options)) != 0) {
            if(FILE * f = fopen("ipip.dat", "w")) {
                fwrite(&option, sizeof(option), 1, f);
                fclose(f);
            }
            lastoption = option;
        }
    }

    void showFigure(int width, int height) {
        static ImVec2 size = ImVec2(400, 200);
        ImVec2 newsize = size;
        int tailn = std::max(1, int(width / size.x));
        int idx = 0;
        for(auto & subp: figure) {
            if(ImGui::Begin(subp.name.c_str())) {
                if(event.tile_window) {
                    ImGui::SetWindowSize(size, ImGuiCond_Always);
                    ImGui::SetWindowPos(ImVec2(idx % tailn * size.x, idx / tailn * size.y), ImGuiCond_Always);
                }
                else {
                    ImGui::SetWindowSize(size, ImGuiCond_FirstUseEver);
                    ImGui::SetWindowPos(ImVec2(idx % tailn * size.x, idx / tailn * size.y), ImGuiCond_FirstUseEver);
                }
                if(ImGui::Button(("FitY##" + subp.name).c_str())) {
                    ImPlot::SetNextPlotLimitsX(0, option.history, ImGuiCond_Always);
                    ImPlot::FitNextPlotAxes(false, true);
                }
                else {
                    ImPlot::SetNextPlotLimitsX(0, option.history, option.lock_x ? ImGuiCond_Always : ImGuiCond_None);
                    ImPlot::SetNextPlotLimitsY(-5, 5);
                }
                bool need_vlim = false;
                for(auto & stream: subp.streams) {
                    if(stream.width > 1) need_vlim = true;
                }
                if(need_vlim) {
                    std::string name = "MinV/MaxV##" + subp.name + "##VLIM";
                    ImGui::SameLine();
                    ImGui::InputFloat2(name.c_str(), subp.scale);
                    ImGui::SameLine();
                    if(ImGui::Button(("FitV##" + subp.name).c_str())) {
                        for(auto & stream: subp.streams) {
                            if(stream.width > 1) {
                                subp.scale[0] = std::min((double)subp.scale[0], stream.vmn);
                                subp.scale[1] = std::max((double)subp.scale[0], stream.vmx);
                            }
                        }
                    }
                }
                std::string plotName = "##" + subp.name + "##PLOT";
                if(event.colormap_changed) {
                    ImPlot::BustColorCache(plotName.c_str());
                }
                if(ImPlot::BeginPlot(plotName.c_str(), NULL, NULL, ImVec2(-1,-1))) {
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
                }
                if(idx == 0) newsize = ImGui::GetWindowSize();
            }
            ImGui::End();
            idx += 1;
        }
        size = newsize;
    }

    Subplot & findSubplot(std::string name) {
        for(auto & subp: figure)
            if(subp.name == name)
                return subp;
        figure.emplace_back(name);
        return figure.back();
    }

    void feedData(Json::Value data) {
        ipipAssert(data.isMember("time") && data["time"].isNumeric(), "time not found", data);
        double tm = data["time"].asDouble();
        for(std::string figName: data.getMemberNames()) {
            if(figName == "time") continue;
            auto & subp = findSubplot(figName);
            if(data[figName].isNull()) continue;
            if(data[figName].isObject()) {
                for(std::string streamName: data[figName].getMemberNames()) {
                    auto & subplotData = data[figName][streamName];
                    if(subplotData.isNull()) continue;
                    subp.findStream(streamName).feed(tm, subplotData);
                }
            }
            else {
                subp.findStream("data").feed(tm, data[figName]);
            }
        }
    }

    void updateWindow(GLFWwindow * window) {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        showSettings();
        Json::Value data;
        while(popQueue(data)) {
            try {
                feedData(data);
            }
            catch(std::runtime_error e) {
                std::cout << "Invalid format: " << e.what() << std::endl << data << std::endl;
            }
        }
        showFigure(width, height);
    }

} // namespace ipip
