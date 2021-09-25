#include"help.h"
#include<cstring>

namespace ipip
{
const char HELP_CONSOLE[] = "\
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
    extern "C" const char HELP_HTML[];
    extern "C" const int HELP_HTML_length;

    static std::string ipipHelpMessage(const char * data, int len, int port) {
        std::string msg(data, data + len);
        for(int i = 0; i < msg.length(); i++) {
            if(msg[i] == '%' && msg[i + 1] == 'd') {
                msg.replace(i, 2, std::to_string(port));
            }
        }
        return msg;
    }

    std::string ipipHtmlHelp(int port) {
        return ipipHelpMessage(HELP_HTML, HELP_HTML_length, port);
    }

    std::string ipipConsoleHelp(int port) {
        return ipipHelpMessage(HELP_CONSOLE, strlen(HELP_CONSOLE), port);
    }

} // namespace ipip