#include "server.h"
#include <queue>
#include <httplib.h>
#include <sstream>
#include "help.h"
#include <thread>

namespace ipip {

    static httplib::Server server;
    static std::thread httpThread;
    static std::queue<Json::Value> serverQueue;
    static std::mutex lock;

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
                res.set_content(ipipHtmlHelp(port), "text/html");
            });
            std::cout << ipipConsoleHelp(port) << std::endl;
            server.listen("0.0.0.0", port);
        });
    }

    void stopServer() {
        server.stop();
        httpThread.join();
    }

}