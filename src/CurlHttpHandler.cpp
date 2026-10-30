
#include "CurlHttpHandler.h"

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    reinterpret_cast<std::string*>(userp)->append(reinterpret_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

std::string CurlHttpHandler::sendGetHTTPBody(const std::string& msg, const std::string& adr, int port) const {
    return sendHTTPRequest("GET", "", "application/json", msg, adr, port);
}

std::string CurlHttpHandler::send(const std::string& msg, const std::string& adr, int port) const {
    return sendHTTPRequest("POST", "", "application/json", msg, adr, port);
}

std::vector<std::string> CurlHttpHandler::sendMulticast(const std::string& /*msg*/, const std::string& /*adr*/,
                                                       int /*port*/, std::chrono::steady_clock::duration /*timeout*/) const {
    return {};
}

std::string CurlHttpHandler::GETString(const std::string& uri, const std::string& contentType, const std::string& body, const std::string& adr, int port) const {
    return sendHTTPRequest("GET", uri, contentType, body, adr, port);
}

std::string CurlHttpHandler::POSTString(const std::string& uri, const std::string& contentType, const std::string& body, const std::string& adr, int port) const {
    return sendHTTPRequest("POST", uri, contentType, body, adr, port);
}

std::string CurlHttpHandler::PUTString(const std::string& uri, const std::string& contentType, const std::string& body, const std::string& adr, int port) const {
    return sendHTTPRequest("PUT", uri, contentType, body, adr, port);
}

std::string CurlHttpHandler::DELETEString(const std::string& uri, const std::string& contentType, const std::string& body, const std::string& adr, int port) const {
    return sendHTTPRequest("DELETE", uri, contentType, body, adr, port);
}

nlohmann::json CurlHttpHandler::GETJson(const std::string& uri, const nlohmann::json& body, const std::string& adr, int port) const {
    return nlohmann::json::parse(GETString(uri, "application/json", body.is_null() ? "" : body.dump(), adr, port));
}

nlohmann::json CurlHttpHandler::POSTJson(const std::string& uri, const nlohmann::json& body, const std::string& adr, int port) const {
    return nlohmann::json::parse(POSTString(uri, "application/json", body.is_null() ? "" : body.dump(), adr, port));
}

nlohmann::json CurlHttpHandler::PUTJson(const std::string& uri, const nlohmann::json& body, const std::string& adr, int port) const {
    return nlohmann::json::parse(PUTString(uri, "application/json", body.is_null() ? "" : body.dump(), adr, port));
}

nlohmann::json CurlHttpHandler::DELETEJson(const std::string& uri, const nlohmann::json& body, const std::string& adr, int port) const {
    return nlohmann::json::parse(DELETEString(uri, "application/json", body.is_null() ? "" : body.dump(), adr, port));
}

std::string CurlHttpHandler::sendHTTPRequest(const std::string& method, const std::string& uri, 
                                           const std::string& contentType, const std::string& body, 
                                           const std::string& adr, int port) const 
{
    CURL* curl = curl_easy_init();
    std::string response;

    if (curl) {
        std::string url = "https://" + adr + ":" + std::to_string(port) + uri;

        struct curl_slist* headers = NULL;
        if (!contentType.empty()) {
            headers = curl_slist_append(headers, ("Content-Type: " + contentType).c_str());
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        if (!body.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        }

        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        CURLcode res = curl_easy_perform(curl);
        
        if (headers) curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            throw std::runtime_error(std::string("cURL error: ") + curl_easy_strerror(res));
        }
    }
    return response;
}