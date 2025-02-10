#include <algorithm>
#include <cstdlib>
#include <future>
#include <iostream>
#include <sstream>
#include <map>
#include <filesystem>
#include <iomanip>

#include "translation.h"
#include "crow.h"
#include "consts.h"
#include "env.h"

// 添加环境变量名称常量
const char* ENV_MODELS_PATH = "MTS_MODELS_PATH";
const char* ENV_API_TOKEN = "MTS_API_TOKEN";
const char* ENV_VERSION = "MTS_VERSION";
const char* ENV_PORT = "MTS_PORT";
const char* ENV_LOG_LEVEL = "MTS_LOG_LEVEL";
const char* ENV_NUM_WORKERS = "MTS_NUM_WORKERS";

std::string getModelsPath() {
    // 首先检查环境变量
    std::string envPath = getEnvVar(ENV_MODELS_PATH, "");
    if (!envPath.empty()) {
        if (std::filesystem::exists(envPath)) {
            return envPath;
        }
        CROW_LOG_WARNING << "Models path from " << ENV_MODELS_PATH << " (" << envPath << ") does not exist";
    }

    // 检查当前目录下的 models
    std::string currentDirModels = "models";
    if (std::filesystem::exists(currentDirModels)) {
        return std::filesystem::absolute(currentDirModels).string();
    }

    // 最后返回 /models
    if (!std::filesystem::exists("/models")) {
        CROW_LOG_WARNING << "Default models path /models does not exist";
    }
    return "/models";
}

std::string escape_json(const std::string &s) {
    std::ostringstream o;
    for (auto c = s.cbegin(); c != s.cend(); c++) {
        if (*c == '"' || *c == '\\' || ('\x00' <= *c && *c <= '\x1f')) {
            o << "\\u"
              << std::hex << std::setw(4) << std::setfill('0') << (int)*c;
        } else {
            o << *c;
        }
    }
    return o.str();
}

// 添加验证token的函数
bool verifyApiToken(const crow::request& req) {
    static const std::string expectedToken = getEnvVar(ENV_API_TOKEN, "");
    if (expectedToken.empty()) {
        return true;  // 如果未设置token，则不进行验证
    }

    std::string_view authHeader = req.get_header_value("Authorization");
    return !authHeader.empty() && authHeader == expectedToken;
}

void run(int port, int workers, crow::LogLevel logLevel) {
    marian::bergamot::TranslatorWrapper wrapper(workers);
    std::string modelsPath = getModelsPath();
    CROW_LOG_INFO << "Loading models from: " << modelsPath;
    wrapper.loadModels(modelsPath);

    crow::SimpleApp app;

    CROW_ROUTE(app, "/v1/translate")
            .methods("POST"_method)
                    ([&wrapper](const crow::request &req) {
                        if (!verifyApiToken(req)) {
                            return crow::response(403);
                        }

                        auto x = crow::json::load(req.body);
                        if (!x) {
                            CROW_LOG_WARNING << "Bad json: " << x;
                            return crow::response(400);
                        }

                        std::string from = (std::string) x["from"];
                        std::string to = (std::string) x["to"];
                        std::string input = (std::string) x["text"];

                        if (!wrapper.isSupported(from, to)) {
                            CROW_LOG_WARNING << "Language pair is not supported: " << from << to;
                            return crow::response(400);
                        }
                        CROW_LOG_DEBUG << "Starting translation from " << from << " to " << to << ": " << input;
                        auto result = wrapper.translate(from, to, input);
                        CROW_LOG_DEBUG << "Finished translation from " << from << " to " << to << ": " << result;

                        // crow json escaping messes up utf-8
                        auto json_result = "{\"result\": \"" + escape_json(result) + "\"}";
                        auto response = crow::response(json_result);
                        response.set_header("Content-Type", "application/json; charset=utf-8");
                        return response;
                    });

    CROW_ROUTE(app, "/v1/languages")
            .methods("GET"_method)
                    ([&wrapper]() {
                        auto pairs = wrapper.getLanguagePairs();
                        crow::json::wvalue response;
                        response["languages"] = pairs;
                        return response;
                    });

    CROW_ROUTE(app, "/version")
            ([] {
                static const std::string version = getEnvVar(ENV_VERSION, __MTS_VERSION__);
                return "{\"version\": \"" + version + "\"}";
            });

    CROW_ROUTE(app, "/__heartbeat__")
            ([] {
                return "Ready";
            });

    CROW_ROUTE(app, "/__lbheartbeat__")
            ([] {
                return "Ready";
            });

    app
    .port(port)
    .loglevel(logLevel)
    .multithreaded()
    .run();
}

int main(int argc, char *argv[]) {
    loadEnvFile();
    auto port = std::stoi(getEnvVar(ENV_PORT, "8989"));

    auto logLevelVar = getEnvVar(ENV_LOG_LEVEL, "INFO");
    auto logLevel = crow::LogLevel::Info;
    if (logLevelVar == "WARNING")
        logLevel = crow::LogLevel::Warning;
    else if (logLevelVar == "ERROR")
        logLevel = crow::LogLevel::Error;
    else if (logLevelVar == "INFO")
        logLevel = crow::LogLevel::Info;
    else if (logLevelVar == "DEBUG")
        logLevel = crow::LogLevel::Debug;
    else {
        throw std::invalid_argument("Unknown logging level: " + logLevelVar);
    }

    auto workers = std::stoi(getEnvVar(ENV_NUM_WORKERS, "1"));
    if (workers == 0)
        workers = std::thread::hardware_concurrency();

    // 在启动时输出所有配置信息
    // std::cout << "Starting Translation Service with configuration:" << std::endl
    //           << "- Port: " << port << std::endl
    //           << "- Log Level: " << logLevelVar << std::endl
    //           << "- Workers: " << workers << std::endl;

    run(port, workers, logLevel);

    return 0;
}
