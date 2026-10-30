#ifndef CURL_HTTP_HANDLER_H
#define CURL_HTTP_HANDLER_H

#include <curl/curl.h>
#include <hueplusplus/IHttpHandler.h>
#include <hueplusplus/Bridge.h>

// #include <hueplusplus/BaseHttpHandler.h>

class CurlHttpHandler : public hueplusplus::IHttpHandler {
public:
    virtual ~CurlHttpHandler() = default;

    std::string sendGetHTTPBody(const std::string& msg, const std::string& adr, int port = 443) const override;
    std::string send(const std::string& msg, const std::string& adr, int port = 443) const override;
    std::vector<std::string> sendMulticast(const std::string& msg, const std::string& adr = "239.255.255.250",
                                           int port = 1900, 
                                           std::chrono::steady_clock::duration timeout = std::chrono::seconds(5)) const override;

    std::string GETString(const std::string& uri, const std::string& contentType, const std::string& body, const std::string& adr, int port = 443) const override;
    std::string POSTString(const std::string& uri, const std::string& contentType, const std::string& body, const std::string& adr, int port = 443) const override;
    std::string PUTString(const std::string& uri, const std::string& contentType, const std::string& body, const std::string& adr, int port = 443) const override;
    std::string DELETEString(const std::string& uri, const std::string& contentType, const std::string& body, const std::string& adr, int port = 443) const override;

    nlohmann::json GETJson(const std::string& uri, const nlohmann::json& body, const std::string& adr, int port = 443) const override;
    nlohmann::json POSTJson(const std::string& uri, const nlohmann::json& body, const std::string& adr, int port = 443) const override;
    nlohmann::json PUTJson(const std::string& uri, const nlohmann::json& body, const std::string& adr, int port = 443) const override;
    nlohmann::json DELETEJson(const std::string& uri, const nlohmann::json& body, const std::string& adr, int port = 443) const override;

    std::string sendHTTPRequest(const std::string& method, const std::string& uri, 
                               const std::string& contentType, const std::string& body, 
                               const std::string& adr, int port) const override;
};

#endif // CURL_HTTP_HANDLER_H
