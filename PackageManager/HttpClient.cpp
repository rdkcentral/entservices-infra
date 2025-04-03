#include <iostream>
#include <math.h>

#include "Module.h"
#include "UtilsLogging.h"

#include "HttpClient.h"

HttpClient::HttpClient() {
    //curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (curl) {
        LOGDBG("curl initialized");
    } else {
        LOGERR("curl initialize failed");
    }
 }

HttpClient::~HttpClient() {
    if (curl) {
        curl_easy_cleanup(curl);
    }
    //curl_global_cleanup();
}

HttpClient::Status
HttpClient::downloadFile(const std::string & url, const std::string & fileName, uint32_t rateLimit) {
    Status status = Status::Success;
    CURLcode cc;
    FILE *fp;
    bCancel = false;
    httpCode = 0;

    if (curl) {
        (void) curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        setRateLimit(rateLimit);

        fp = fopen(fileName.c_str(), "wb");
        if (fp != NULL) {
            (void) curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
            (void) curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

            (void) curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            (void) curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, this);
            (void) curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, progressCb);

            cc = curl_easy_perform(curl);
            curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &httpCode);
            if (cc == CURLE_OK) {
                LOGDBG("Download %s Success", fileName.c_str());
            } else {
                LOGERR("Download %s Failed error: %s code: %ld", fileName.c_str(), curl_easy_strerror(cc), httpCode);
                status = Status::HttpError;
            }
            fclose(fp);
        } else {
            LOGERR("Failed to open %s", fileName.c_str());
            status = Status::DiskError;
        }
    }

    return status;
}

size_t HttpClient::progressCb(void *ptr, double dltotal, double dlnow, double ultotal, double ulnow)
{
    HttpClient *pHttpClient = static_cast<HttpClient *>(ptr);

    int percent = (int)(dlnow * 100 / dltotal);
    pHttpClient->progress = percent;
    // LOGTRACE("%d completed", percent);

    return pHttpClient->bCancel;
}

size_t HttpClient::write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    // LOGTRACE("size=%ld nmemb=%ld", size, nmemb);
    return fwrite(ptr, size, nmemb, stream);
}

