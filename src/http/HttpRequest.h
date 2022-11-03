/*
 * @Author: starrysky9959 965105951@qq.com
 * @Date: 2022-10-19 00:14:39
 * @LastEditors: starrysky9959 965105951@qq.com
 * @LastEditTime: 2022-11-02 18:50:56
 * @Description: 
 */
#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include "../buffer/Buffer.h"

enum PARSE_STATE {
    REQUEST_LINE,
    HEADER,
    // BLANK_LINE,
    BODY,
    FINISH,
};

/*
 * HTTP请求报文由请求行request line, 请求头部header, 空行和请求数据四个部分组成
*/
class HttpRequest {
public:
    HttpRequest();
    ~HttpRequest() = default;

    void init();
    bool parse(Buffer &buffer);

    // PARSE_STATE getState;
    std::string getMethod() const;
    std::string getPath() const;
    std::string& getPath() ;
    std::string getVersion() const;
    bool isKeepAlive() const;

private:
    void parsePath();
    bool parseRequestLine(const std::string &line);
    void parseHeader(const std::string &line);
    void parseBody(const std::string &line);

    PARSE_STATE state_;
    std::string method_;
    std::string path_;
    std::string version_;
    std::unordered_map<std::string, std::string> header_;
    std::string body_;
  
    static const std::unordered_set<std::string> DEFAULT_HTML;
};
