#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

// 系统调用头文件
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <sys/wait.h>

// C++标准库头文件
#include <string>
#include <memory>
#include <sstream>
#include <cstdlib>
#include <unordered_map>
#include <algorithm>
#include <iostream>
#include <vector>

// 自定义接口或自定义类
#include "util.hpp"
#include "log.hpp"

// 全部变量
static const std::string SEP = ": ";
static const std::string WEB_ROOT = "./wwwroot";
static const std::string HOME_PAGE = "index.html";
static const std::string HTTP_VERSION = "HTTP/1.0";
static const std::string LINE_END = "\r\n";
static const std::string PAGE_404 = "404.html";

// 响应状态码
enum StatusCode{
    OK = 200,
    BAD_REQUEST = 400,
    NOT_FOUND = 404,
    SERVER_ERROR = 500
};

// 全局函数,作用是：将状态码转换为对应的描述
static std::string Code2Desc(int code) {
    static std::unordered_map<int, std::string> code_2_desc;
    code_2_desc[OK] = "OK";
    code_2_desc[NOT_FOUND] = "Not Found";

    return code_2_desc[code];
}

// 全局函数，Content-Type对照表
static std::string Suffix2Desc(const std::string& suffix) {
    static std::unordered_map<std::string, std::string> desc;
    desc["default"] = "application/octet-stream";
    desc[".html"] = "text/html";
    desc[".ico"] = "image/x-icon";
    desc[".img"] = "application/x-img";
    desc[".jpg"] = "application/x-jpg";
    desc[".js"] = "application/x-javascript";
    desc[".css"] = "text/css";
    desc[".gif"] = "image/gif";
    desc[".mp3"] = "audio/mp3";
    desc[".png"] = "image/png";

    if (!desc.count(suffix)) {
        std::string msg = "not found ";
        msg += suffix;
        LOG(WARNING, msg);
        return desc["default"];
    }
    return desc[suffix];
}

// Http请求的相关内容
class HttpRequest{
public:
    std::string request_line_;
    std::vector<std::string> request_header_;
    std::string blank_;
    std::string request_body_;

    // 解析后的结果
    std::string method_;
    std::string uri_;
    std::string version_;

    std::unordered_map<std::string, std::string> header_kv;

    int content_length_ = 0;

    std::string path_;
    std::string query_string_;
    std::string suffix_;

    int request_size_ = 0;

    bool cgi_ = false;
};

// Http响应的相关内容
class HttpResponse{
public:
    std::string status_line_;
    std::vector<std::string> response_header_;
    std::string blank_ = LINE_END;
    std::string response_body_;

    int status_ = OK;
    int in_fd_ = -1;
};

// 处理Http请求和构建Http响应
class EndPoint{
public:
    EndPoint(int sock) : sockfd_(sock) {}
    ~EndPoint() {close(sockfd_); }

    bool Stop() const {
        return stop_;
    }

    void RecvHttpRequest() {
        // RecvHttpRequestLine();
        // RecvHttpRequestHeader();

        if ((!RecvHttpRequestLine()) && (!RecvHttpRequestHeader())) {
            ParseHttpRequsetLine();
            ParseHttpRequestHeader();
            RecvHttpRequestBody();
        }
    }


    void BuildHttpResponse() {
        int &code = http_response_.status_;
        struct stat st;
        std::string path = WEB_ROOT;
        // int size = 0;
        size_t found = 0;

        if (http_request_.method_ != "GET" && http_request_.method_ != "POST") {
            LOG(WARNING, "method is not right!");
            code = BAD_REQUEST;
            goto END;
        }

        // 只处理GET和POST
        if (http_request_.method_ == "GET") {
            size_t pos = http_request_.uri_.find('?');
            if (pos != std::string::npos) {
                Util::CutString(http_request_.uri_, http_request_.path_, http_request_.query_string_, "?");
                http_request_.cgi_ = true;
            }else{
                http_request_.path_ = http_request_.uri_;
            }
        }else if (http_request_.method_ == "POST") {
            http_request_.cgi_ = true;

            http_request_.path_ = http_request_.uri_;

        }else{
            // do nothing
        }
        path += http_request_.path_;
        http_request_.path_ = path;
        
        if (http_request_.path_[http_request_.path_.size()-1] == '/') {
            http_request_.path_ += HOME_PAGE;
        }
        if (stat(http_request_.path_.c_str(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                http_request_.path_ += '/';
                http_request_.path_ += HOME_PAGE;
                stat(http_request_.path_.c_str(), &st);
            }

            if ((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH)) {
                // 需要进行特殊处理
                http_request_.cgi_ = true;
            }
            // size = st.st_size;
            http_request_.request_size_ = st.st_size;

        }else{
            std::string info = http_request_.path_;
            info += " Not Found!";
            LOG(WARNING, info);
            code = NOT_FOUND;
            goto END;
        }

        found = http_request_.path_.rfind('.');
        if (found == std::string::npos) {
            http_request_.suffix_ = ".html";
        }else{
            http_request_.suffix_ = http_request_.path_.substr(found);
        }

        if (http_request_.cgi_) {
            code = ProcessCgi();
        }else{
            code = ProcessNoneCgi();
        }

END:
        BuildHttpResponseHelper();

        // std::cout << "debug: uri--> " << http_request_.uri_ << std::endl;
        // std::cout << "debug: path--> " << http_request_.path_ << std::endl;
        // std::cout << "debug: args--> " << http_request_.query_string_ << std::endl;
    }


    void SendHttpResponse() {
        // 发送状态行
        send(sockfd_, http_response_.status_line_.c_str(), http_response_.status_line_.size(), 0);

        // 发送响应报头
        LOG(INFO, "start send header");
        for (const std::string &s : http_response_.response_header_) {
            send(sockfd_, s.c_str(), s.size(), 0);
            LOG(INFO, s);
        }
        // 发送空行
        send(sockfd_, http_response_.blank_.c_str(), http_response_.blank_.size(), 0);
        LOG(INFO, http_response_.blank_);
        LOG(INFO, "start send body");
        if (http_request_.cgi_) {
            std::string &response_body = http_response_.response_body_;
            size_t total = 0;
            const char* start = response_body.c_str();
            // LOG(INFO, response_body);
            while (true) {
                if (total >= response_body.size()) {
                    break;
                }
                size_t size = send(sockfd_, start+total, response_body.size()-total, 0);
                if (size <= 0) {
                    break;
                }
                total += size;
            }
        }else{
            sendfile(sockfd_, http_response_.in_fd_, nullptr, http_request_.request_size_);
            close(http_response_.in_fd_);
        }
    }

protected:
    void BuilOKResponse() {
        std::string line;

        line = "Content-Type: ";
        line += Suffix2Desc(http_request_.suffix_);
        line += LINE_END;
        http_response_.response_header_.push_back(line);

        line = "Content-Length: ";
        if (http_request_.cgi_) {
            line += std::to_string(http_response_.response_body_.size());
        }else{
            line += std::to_string(http_request_.request_size_);
        }
        line += LINE_END;
        http_response_.response_header_.push_back(line);
    }

    void HandlerError(const std::string &path) {
        http_request_.cgi_ = false;
        http_response_.in_fd_ = open(path.c_str(), O_RDONLY);

        if (http_response_.in_fd_ > 0) {
            struct stat st;
            stat(path.c_str(), &st);
            http_request_.request_size_ = st.st_size;
            std::string line;

            line = "Content-Type: text/html";
            line += LINE_END;
            http_response_.response_header_.push_back(line);

            line = "Content-Length: ";
            line += std::to_string(st.st_size);
            line += LINE_END;
            http_response_.response_header_.push_back(line);
        }
    }

    void BuildHttpResponseHelper() {
        int &code = http_response_.status_;
        std::string &status_line = http_response_.status_line_;
        status_line.clear();
        status_line += HTTP_VERSION;
        status_line += " ";
        status_line += std::to_string(code);
        status_line += " ";
        status_line += Code2Desc(code);
        status_line += LINE_END;

        std::string path = WEB_ROOT;
        path +="/";

        switch (code) {
        case OK:
            BuilOKResponse();
            break;
        case NOT_FOUND:
            path += PAGE_404;
            HandlerError(path);
            break;
        case BAD_REQUEST:
            path += PAGE_404;
            HandlerError(path);
            break;
        case SERVER_ERROR:
            path += PAGE_404;
            HandlerError(path);
            break;
        default:
            break;
        }
    }

    int ProcessCgi() {
        // "管道的读写完全站在父进程的角度"
        int code = OK;
        std::string &bin = http_request_.path_;
        std::string &qurey_string = http_request_.query_string_;
        std::string &body_text = http_request_.request_body_;
        std::string &method = http_request_.method_;
        std::string env_args;
        std::string method_env;
        std::string content_length_env;
        std::string &response_body = http_response_.response_body_;
        int content_length = http_request_.content_length_;

        int input[2] = {0};
        int output[2] = {0};

        if (pipe(input) < 0){
            LOG(ERROR, "pipe error");
            code = SERVER_ERROR;
            return code;
        }
        if (pipe(output) < 0) {
            LOG(ERROR, "pipe error");
            code = SERVER_ERROR;
            return code;
        }

        pid_t pid = fork();
        if (pid == 0) {
            close(input[0]);
            close(output[1]);
            // 站在子进程的角度,可用文件描述符：input[1](写出)、output[0](读入)
            method_env = "METHOD=";
            method_env += method;
            putenv(const_cast<char*>(method_env.c_str()));

            if (method == "GET") {
                env_args = "QUERY_STRING=";
                env_args += qurey_string;
                putenv(const_cast<char*>(env_args.c_str()));
            }else if (method == "POST") {
                content_length_env = "CONTENT_LENGTH=";
                content_length_env += std::to_string(content_length);
                putenv(const_cast<char*>(content_length_env.c_str()));
            }else{
                LOG(WARNING, "Method: !POST || !GET");
            }

            dup2(output[0], 0);
            dup2(input[1], 1);

            execl(bin.c_str(), bin.c_str(), nullptr);
            exit(1);
        }else if (pid < 0) {
            LOG(ERROR, "fork error");
            return NOT_FOUND;
        }

        close(input[1]);
        close(output[0]);

        if ("POST" == method) {
            const char* start = body_text.c_str();
            int total = 0;
            int size = 0;
            
            // while (size = write(output[1], start+total, body_text.size()-total) > 0) {
            //     total += size;
            // }

            while (true) {
                if (total >= content_length) {
                    break;
                }
                size = write(output[1], start+total, body_text.size()-total);
                if (size <= 0) {
                    break;
                }
                total += size;
            }
        }

        std::cout << "input[0] --> " << input[0] << std::endl; 
        char ch = 0;
        while (read(input[0], &ch, 1) > 0) {
            response_body += ch;
        }

        std::cout << "Debug CGI: --> " << response_body << std::endl;

        int status = 0;
        pid_t ret = waitpid(pid, &status, 0);
        if (ret == pid) {
            if (WIFEXITED(status)) {
                std::cout << "child exit code --> " << WIFEXITED(status) << std::endl;

                // 第一个大BUG，没有判断是否等于0
                // if (WEXITSTATUS(status)) {...},

                if (0 == WEXITSTATUS(status)) {
                    code = OK;
                }else{
                    code = BAD_REQUEST;
                }
            }else{
                // std::cout << "child exit code --> " << WIFEXITED(status) << std::endl;
                code = SERVER_ERROR;
            }
        }

        std::cout << "[CGIed] --> code: " << code << std::endl;

        close(input[0]);
        close(output[1]);
        return code;
    }

    int ProcessNoneCgi() {
        // 状态行构建
        http_response_.in_fd_ = open(http_request_.path_.c_str(), O_RDONLY);
        if (http_response_.in_fd_ > 0) {
            // http_response_.status_line_ = HTTP_VERSION;
            // http_response_.status_line_ += " ";
            // http_response_.status_line_ += std::to_string(http_response_.status_);
            // http_response_.status_line_ += " ";
            // http_response_.status_line_ += Code2Desc(http_response_.status_);
            // http_response_.status_line_ += LINE_END;
            // http_response_.body_size_ = file_size;

            // std::string header_line;

            // header_line = "Content-Type: ";
            // header_line += Suffix2Desc(http_request_.suffix_);
            // header_line += LINE_END;
            // http_response_.response_header_.push_back(header_line);

            // header_line = "Content-Length: ";
            // header_line += std::to_string(http_response_.body_size_);
            // header_line += LINE_END;
            // http_response_.response_header_.push_back(header_line);

            return OK;
        }
        return NOT_FOUND;
    }

    bool RecvHttpRequestBody() {
        if (isNeedRecvHttpRequestBody()) {
            int content_length = http_request_.content_length_;
            std::string &body = http_request_.request_body_;
            char ch = 0;
            while (content_length) {
                ssize_t s = recv(sockfd_, &ch, 1, 0);
                if (s > 0) {
                    body += ch;
                    --content_length;
                }else{
                    stop_ = true;
                    break;
                }
            }
            LOG(INFO, body);
        }
        return stop_;
    }

    bool isNeedRecvHttpRequestBody() {
        std::string &method = http_request_.method_;
        if ("POST" == method) {
            std::unordered_map<std::string, std::string> header_kv = http_request_.header_kv;
            std::unordered_map<std::string, std::string>::iterator iter = header_kv.find("Content-Length");
            if (iter != header_kv.end()) {
                http_request_.content_length_ = std::atoi(iter->second.c_str());
                return true;
            }
        }
        return false;
    }

    void ParseHttpRequestHeader() {
        std::string key_out;
        std::string value_out;
        for (const std::string &iter : http_request_.request_header_) {
            if (Util::CutString(iter, key_out, value_out, SEP)) {
                std::cout << "debug--> " << key_out << SEP << value_out << std::endl;
                http_request_.header_kv[key_out] = value_out;
            }
        }
    }

    bool RecvHttpRequestLine() {
        std::string line;
        if (Util::ReadLine(sockfd_, line) > 0) {
            line.resize(line.size()-1);
            http_request_.request_line_ = line;
            LOG(INFO, http_request_.request_line_);
        }else{
            stop_ = true;
        }
        return stop_;
    }

    void ParseHttpRequsetLine() {
        std::string &line = http_request_.request_line_;
        std::stringstream ss(line);
        ss >> http_request_.method_ >> http_request_.uri_ >> http_request_.version_;
        std::string &method = http_request_.method_;

        // std::transform(method.begin(), method.end(), method, ::toupper);

        for (size_t i = 0; i < method.size(); ++i) {
            toupper(method[i]);
        }

        LOG(INFO, http_request_.method_);
        LOG(INFO, http_request_.uri_);
        LOG(INFO, http_request_.version_);
    }

    bool RecvHttpRequestHeader() {
        std::string line;
        while (1) {
            line.clear();
            if (Util::ReadLine(sockfd_, line) <= 0) {
                stop_ = true;
                break;
            }

            if (line == "\n") {
                http_request_.blank_ = line;
                break;
            }
            line.resize(line.size()-1);
            http_request_.request_header_.push_back(line);
            LOG(INFO, line);
        }
        if (line == "\n") {
            LOG(INFO, http_request_.blank_);
        }
        return stop_;
    }

protected:
    int sockfd_; // 本地网络套接字
    HttpRequest http_request_;
    HttpResponse http_response_;
    bool stop_ = false;
};


#define DEBUG

class CallBack{
public:
    void operator()(int sock) {
        HandlerRequest(sock);
    }

    void HandlerRequest(int sock) {
        LOG(INFO, "hander request begin");
        //std::cout << "get a new link ... : " << sock << std::endl;
#ifdef DEBUG
        // char buffer[4*1024] = {0};
        // recv(sock, buffer, sizeof(buffer), 0);
        // std::cout << "------------------begin------------------------------" << std::endl;
        // std::cout << buffer << std::endl;
        // std::cout << "-------------------end-------------------------------" << std::endl << std::endl;

        std::shared_ptr<EndPoint> ep{new EndPoint{sock}};
        ep->RecvHttpRequest();
        //ep->ParseHttpRequset();
        if (!ep->Stop()) {
            LOG(INFO, "Recv No Error, Begin Build And Send");
            ep->BuildHttpResponse();
            ep->SendHttpResponse();
        }else{
            LOG(INFO, "Recv Error, Stop Build And Send");
        }
#endif
        LOG(INFO, "hander request end");
    }
};

#endif