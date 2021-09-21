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
#include <queue>
#include <httplib.h>

namespace ipip {

const char help_msg[] = 
"<html>\
<body>\
<h1>Welcome to ipip Server</h1>\
<p>Please Post this url to submit data\
</body>\
</html>\
";

    httplib::Server server;
    std::thread httpThread;
    std::queue<Json::Value> serverQueue;
    std::mutex lock;

    bool popQueue(Json::Value & result) {
        if(serverQueue.empty()) {
            return false;
        }
        lock.lock();
        result = serverQueue.front();
        serverQueue.pop();
        lock.unlock();
        return true;
    }

    void initServer(int port) {
        httpThread = std::thread([&,port]() {
            using namespace httplib;
            server.Post("/", [&](const Request &req, Response &res, const ContentReader &content_reader) {
                if (req.is_multipart_form_data()) {
                    throw std::runtime_error("not implemented");
                } else {
                    std::string body;
                    content_reader([&](const char *data, size_t data_length) {
                        body.append(data, data_length);
                        return true;
                    });
                    Json::Value root;
                    Json::Reader reader;
                    bool parsingSuccessful = reader.parse(body, root);
                    if (parsingSuccessful) {
                        lock.lock();
                        serverQueue.push(root);
                        lock.unlock();
                    }
                    else {
                        std::cout << "Error when parsing json" << "\n" << body << '\n';
                    }
                }
            });
            server.Get("/", [](const Request& req, Response& res) {
                res.set_content(help_msg, "text/plain");
            });
            std::cout << "Binding server on http://localhost:" << port << std::endl;
            server.listen("0.0.0.0", port);
        });
    }

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
                "", ImPlotPoint(tickmod.size() ? tickmod.front() : 0, 0), ImPlotPoint(tickmod.size() ? tickmod.back() : 0, 1));
        }

        void feed(double time, const std::vector<double> & value) {
            updateBuffer(time);
            width = value.size();
            std::copy(value.begin(), value.end(), std::back_inserter(data));
        }

        void feed(double time, Json::Value value) {
            assert(value.isArray() || value.isNumeric());
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
        assert(data.isMember("time") && data["time"].isNumeric());
        double tm = data["time"].asDouble();
        for(std::string figName: data.getMemberNames()) {
            if(figName == "time") continue;
            auto & subp = findSubplot(figName);
            for(std::string streamName: data[figName].getMemberNames()) {
                subp.findStream(streamName).feed(tm, data[figName][streamName]);
            }
        }
    }

    void updateWindow() {
        showSettings();
        Json::Value data;
        while(popQueue(data)) {
            try {
                feedData(data);
            }
            catch(std::runtime_error e) {
                std::cout << "Invalid format: " << data << std::endl;
            }
        }
        showFigure();
    }

} // namespace ipip
