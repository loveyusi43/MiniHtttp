#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <sys/wait.h>

#include <string>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <algorithm>
#include <iostream>
#include <vector>

#include "util.hpp"
#include "log.hpp"

const std::string SEP = ": ";
const std::string WEB_ROOT = "./wwwroot";
const std::string HOME_PAGE = "index.html";
const std::string HTTP_VERSION = "HTTP/1.0";
const std::string LINE_END = "\r\n";

enum Status{
    OK = 200,
    NOT_FOUND = 404
};

static std::string Code2Desc(int code) {
    static std::unordered_map<int, std::string> code_2_desc;
    code_2_desc[OK] = "OK";
    code_2_desc[NOT_FOUND] = "Not Found";

    return code_2_desc[code];
}

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

class HttpRequset{
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

    bool cgi_ = false;
};

class HttpResponse{
public:
    std::string status_line_;
    std::vector<std::string> response_header_;
    std::string blank_ = LINE_END;
    std::string response_body_;

    int status_ = OK;

    int in_fd_ = -1;
    int body_size_ = 0;
};

class EndPoint{
public:
    EndPoint(int sock) : sockfd_(sock) {}
    ~EndPoint() {close(sockfd_); }

    void RecvHttpRequest() {
        RecvHttpRequestLine();
        ParseHttpRequsetLine();

        RecvHttpRequestHeader();
        ParseHttpRequestHeader();

        RecvHttpRequestBody();
    }


    void BuildHttpResponse() {
        int &code = http_response_.status_;
        struct stat st;
        std::string path = WEB_ROOT;
        int size = 0;
        size_t found = 0;

        if (http_requset_.method_ != "GET" && http_requset_.method_ != "POST") {
            LOG(WARNING, "method is not right!");
            code = NOT_FOUND;
            return;
        }

        // 只处理GET和POST
        if (http_requset_.method_ == "GET") {
            size_t pos = http_requset_.uri_.find('?');
            if (pos != std::string::npos) {
                Util::CutString(http_requset_.uri_, http_requset_.path_, http_requset_.query_string_, "?");
                http_requset_.cgi_ = true;
            }else{
                http_requset_.path_ = http_requset_.uri_;
            }
        }else if (http_requset_.method_ == "POST") {
            http_requset_.cgi_ = true;
        }else{
            // do nothing
        }
        path += http_requset_.path_;
        http_requset_.path_ = path;
        
        if (http_requset_.path_[http_requset_.path_.size()-1] == '/') {
            http_requset_.path_ += HOME_PAGE;
        }
        if (stat(http_requset_.path_.c_str(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                http_requset_.path_ += '/';
                http_requset_.path_ += HOME_PAGE;
                stat(http_requset_.path_.c_str(), &st);
            }

            if ((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH)) {
                // 需要进行特殊处理
                http_requset_.cgi_ = true;
            }
            size = st.st_size;

        }else{
            std::string info = http_requset_.path_;
            info += " Not Found!";
            LOG(WARNING, info);
            code = NOT_FOUND;
            return;
        }

        found = http_requset_.path_.rfind('.');
        if (found == std::string::npos) {
            http_requset_.suffix_ = ".html";
        }else{
            http_requset_.suffix_ = http_requset_.path_.substr(found);
        }

        if (http_requset_.cgi_) {
            // ProcessCgi();
        }else{
            code = ProcessNoneCgi(size);
        }

        if (code != OK) {
            ;
        }

        std::cout << "debug: uri--> " << http_requset_.uri_ << std::endl;
        std::cout << "debug: path--> " << http_requset_.path_ << std::endl;
        std::cout << "debug: args--> " << http_requset_.query_string_ << std::endl;
    }


    void SendHttpResponse() {
        send(sockfd_, http_response_.status_line_.c_str(), http_response_.status_line_.size(), 0);
        //send(sockfd_, http_response_.response_header_.c_str(), http_response_.response_header_.size(), 0);
        for (const std::string &s : http_response_.response_header_) {
            send(sockfd_, s.c_str(), s.size(), 0);
        }
        send(sockfd_, http_response_.blank_.c_str(), http_response_.blank_.size(), 0);
        //send(sockfd_, http_response_.response_body_.c_str(), http_response_.response_body_.size(), 0);
        sendfile(sockfd_, http_response_.in_fd_, nullptr, http_response_.body_size_);
        close(http_response_.in_fd_);
    }

protected:
    int ProcessCgi() {
        // "管道的读写完全站在父进程的角度"
        int code = OK;
        std::string &bin = http_requset_.path_;
        std::string &qurey_string = http_requset_.query_string_;
        std::string &body_text = http_requset_.request_body_;
        std::string &method = http_requset_.method_;

        int input[2] = {0};
        int output[2] = {0};

        if (pipe(input) < 0){
            LOG(ERROR, "pipe error");
            code = NOT_FOUND;
            return code;
        }

        if (pipe(output) < 0) {
            LOG(ERROR, "pipe error");
            code = NOT_FOUND;
            return code;
        }

        pid_t pid = fork();
        if (pid == 0) {
            close(input[0]);
            close(output[1]);
            // 站在子进程的角度,可用文件描述符：input[1](写出)、output[0](读入)
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
            while (size = write(output[1], start+total, body_text.size()-total) > 0) {
                total += size;
            }
        }

        waitpid(pid, nullptr, 0);
        close(input[0]);
        close(output[1]);
        return OK;
    }

    int ProcessNoneCgi(int file_size) {
        // 状态行构建
        http_response_.in_fd_ = open(http_requset_.path_.c_str(), O_RDONLY);
        if (http_response_.in_fd_ > 0) {
            http_response_.status_line_ = HTTP_VERSION;
            http_response_.status_line_ += " ";
            http_response_.status_line_ += std::to_string(http_response_.status_);
            http_response_.status_line_ += " ";
            http_response_.status_line_ += Code2Desc(http_response_.status_);
            http_response_.status_line_ += LINE_END;
            http_response_.body_size_ = file_size;

            std::string header_line;

            header_line = "Content-Type: ";
            header_line += Suffix2Desc(http_requset_.suffix_);
            header_line += LINE_END;
            http_response_.response_header_.push_back(header_line);

            header_line = "Content-Length: ";
            header_line += std::to_string(http_response_.body_size_);
            header_line += LINE_END;
            http_response_.response_header_.push_back(header_line);

            return OK;
        }
        return NOT_FOUND;
    }

    void RecvHttpRequestBody() {
        int content_length = http_requset_.content_length_;
        std::string &body = http_requset_.request_body_;
        char ch = 0;
        while (content_length) {
            ssize_t s = recv(sockfd_, &ch, 1, 0);
            if (s > 0) {
                body += ch;
                --content_length;
            }else{
                break;
            }
        }
    }

    bool isNeedRecvHttpRequestBody() {
        std::string &method = http_requset_.method_;
        if ("POST" == method) {
            std::unordered_map<std::string, std::string> header_kv = http_requset_.header_kv;
            std::unordered_map<std::string, std::string>::iterator iter = header_kv.find("Content-Length");
            if (iter != header_kv.end()) {
                http_requset_.content_length_ = std::atoi(iter->second.c_str());
                return true;
            }
        }
        return false;
    }

    void ParseHttpRequestHeader() {
        std::string key_out;
        std::string value_out;
        for (const std::string &iter : http_requset_.request_header_) {
            if (Util::CutString(iter, key_out, value_out, SEP)) {
                std::cout << "debug--> " << key_out << SEP << value_out << std::endl;
                http_requset_.header_kv[key_out] = value_out;
            }
        }
    }

    void RecvHttpRequestLine() {
        std::string line;
        Util::ReadLine(sockfd_, line);
        line.resize(line.size()-1);
        http_requset_.request_line_ = line;
        LOG(INFO, http_requset_.request_line_);
    }

    void ParseHttpRequsetLine() {
        std::string &line = http_requset_.request_line_;
        std::stringstream ss(line);
        ss >> http_requset_.method_ >> http_requset_.uri_ >> http_requset_.version_;
        std::string &method = http_requset_.method_;

        // std::transform(method.begin(), method.end(), method, ::toupper);

        for (size_t i = 0; i < method.size(); ++i) {
            toupper(method[i]);
        }

        LOG(INFO, http_requset_.method_);
        LOG(INFO, http_requset_.uri_);
        LOG(INFO, http_requset_.version_);
    }

    void RecvHttpRequestHeader() {
        std::string line;
        while (1) {
            line.clear();
            Util::ReadLine(sockfd_, line);
            if (line == "\n") {
                http_requset_.blank_ = line;
                break;
            }
            line.resize(line.size()-1);
            http_requset_.request_header_.push_back(line);
            LOG(INFO, line);
        }
        if (line == "\n") {
            LOG(INFO, http_requset_.blank_);
        }
    }

protected:
    int sockfd_;
    HttpRequset http_requset_;
    HttpResponse http_response_;
};

#define DEBUG

class Entrance{
public:
    static void HandlerRequest(int sock) {
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
        ep->BuildHttpResponse();
        ep->SendHttpResponse();
#endif
        LOG(INFO, "hander request end");
    }
};

#endif