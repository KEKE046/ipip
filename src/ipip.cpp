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
#include <sstream>

namespace ipip {

const char help_msg[] = "\
** Welcome to IPIP Server\n\
\n\
    Server running on `http://127.0.0.1:%d`\n\
\n\
--------------------------------------\n\
** matlab code:\n\
\n\
url='http://127.0.0:%d';\n\
while true\n\
    time = now * 60 * 60 * 24;\n\
    data = struct(...\n\
        'time', time,...\n\
        'fig1', struct(...\n\
            'sin', sin(time)...\n\
        )...\n\
    );\n\
    webwrite(url, data);\n\
    pause(0.01);\n\
end\n\
\n\
--------------------------------------\n\
** python code:\n\
\n\
import requests, json, time, math\n\
\n\
url = 'http://127.0.0.1:%d'\n\
sess = requests.Session()\n\
\n\
while True:\n\
    sess.post(url, data=json.dumps({\n\
        'time': time.time(),\n\
        'fig1': {\n\
            'sin': math.sin(time.time())\n\
        }\n\
    }))\n\
    time.sleep(0.01)\n\
";

    std::string ipipHelpMessage(int port) {
        auto buf = new char[strlen(help_msg) + 100];
        sprintf(buf, help_msg, port, port, port);
        std::string res(buf);
        delete [] buf;
        return res;
    }

    static httplib::Server server;
    static std::thread httpThread;
    static std::queue<Json::Value> serverQueue;
    static std::mutex lock;

    static void _ipipConcatMessage(std::stringstream & ss) {}

    template<class T1, class ... T>
    static void _ipipConcatMessage(std::stringstream & ss, const T1 & msg, const T & ...  msgs) {
        ss << msg; _ipipConcatMessage(ss, msgs ...);
    }

    template<class ... T>
    void ipipAssert(bool cond, T ... msg) {
        if(!cond) {
            std::stringstream ss;
            _ipipConcatMessage(ss, msg ...);
            throw std::runtime_error(ss.str());
        }
    }

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
                    Json::String errors;
                    Json::CharReaderBuilder builder;
                    std::unique_ptr<Json::CharReader> const reader(builder.newCharReader());
                    bool success = reader->parse(body.data(), body.data() + body.length(), &root, &errors);
                    if (!success || !errors.empty()) {
                        std::cout << "Error parsing json" << '\n' << body << '\n' << errors << '\n';
                    }
                    else {
                        lock.lock();
                        serverQueue.push(root);
                        lock.unlock();
                    }
                }
            });
            server.Get("/", [=](const Request& req, Response& res) {
                res.set_content(ipipHelpMessage(port), "text/plain");
            });
            // std::cout << "Binding server on http://localhost:" << port << std::endl;
            std::cout << ipipHelpMessage(port) << std::endl;
            server.listen("0.0.0.0", port);
        });
    }

    void stopServer() {
        server.stop();
        httpThread.join();
    }

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
        double mx{-1e100}, mn{1e100};
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
                mx = -1e100;
                mn = +1e100;
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
            if(width == 1) {
                mx = std::max(mx, (value[0]));
                mn = std::min(mn, (value[0]));
            }
            else{
                mn = 0;
                mx = 1;
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
        ImGui::SetNextWindowSize(ImVec2(600, 750), ImGuiCond_FirstUseEver);
        ImGui::Begin("Setting");
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
                if(ImGui::Button(("AutoResize##" + subp.name).c_str())) {
                    double mn = 1e100, mx = -1e100;
                    for(auto & stream: subp.streams) {
                        mn = std::min(mn, stream.mn);
                        mx = std::max(mx, stream.mx);
                    }
                    if(mn > mx) {mn = -5; mx = 5;}
                    ImPlot::SetNextPlotLimitsX(0, option.history, ImGuiCond_Always);
                    ImPlot::SetNextPlotLimitsY(mn, mx, ImGuiCond_Always);
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
                    std::string name = "Min / Max##" + subp.name + "##VLIM";
                    ImGui::InputFloat2(name.c_str(), subp.scale);
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
                if(idx == 0) {
                    newsize = ImGui::GetWindowSize();
                }
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
            if(data[figName].isObject()) {
                for(std::string streamName: data[figName].getMemberNames()) {
                    subp.findStream(streamName).feed(tm, data[figName][streamName]);
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
            std::cout << data << std::endl;
            try {
                feedData(data);
            }
            catch(std::runtime_error e) {
                std::cout << "Invalid format: " << data << std::endl;
            }
        }
        showFigure(width, height);
    }

} // namespace ipip
