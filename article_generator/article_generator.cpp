#include "httplib.h"
#include <nlohmann/json.hpp>
#include <curl/curl.h>

#include <cstdlib>
#include <atomic>
#include <string>
#include <vector>

using json = nlohmann::json;

// ── Topics ──────────────────────────────────────────────────────────────────

static const std::vector<std::string> TOPICS = {
    "the surprising history of everyday objects",
    "how bees navigate using the sun",
    "the science of why we yawn",
    "ancient civilizations lost to time",
    "the physics of rainbows",
    "why do cats purr",
    "the mystery of deep sea creatures",
    "how languages evolve over centuries",
    "the psychology of color perception",
    "unusual animal friendships in the wild",
};

static std::atomic<int> topic_index{0};

// ── libcurl helpers ──────────────────────────────────────────────────────────

static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
    auto *out = static_cast<std::string *>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

// Call the Anthropic Messages API and return the generated text.
// Returns empty string on failure.
static std::string call_claude(const std::string &api_key,
                               const std::string &topic) {
    CURL *curl = curl_easy_init();
    if (!curl) return "";

    // Build request body
    json body = {
        {"model", "claude-opus-4-6"},
        {"max_tokens", 1024},
        {"messages", {
            {{"role", "user"},
             {"content", "Write a short, engaging article (3-4 paragraphs) about: "
                         + topic +
                         ". Include a catchy title at the top prefixed with '# '. "
                         "Keep it informative and accessible."}}
        }}
    };
    std::string body_str = body.dump();

    // Headers
    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, ("x-api-key: " + api_key).c_str());
    headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");

    std::string response_body;
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.anthropic.com/v1/messages");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body_str.size());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) return "";

    try {
        auto resp = json::parse(response_body);
        return resp["content"][0]["text"].get<std::string>();
    } catch (...) {
        return "";
    }
}

// ── Embedded HTML ────────────────────────────────────────────────────────────

static const char *HTML = R"html(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Article Generator</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: Georgia, serif;
      background: #f5f0e8;
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      align-items: center;
      padding: 40px 20px;
    }
    h1 { font-size: 2rem; color: #2c2c2c; margin-bottom: 8px; }
    .subtitle { color: #777; margin-bottom: 32px; font-style: italic; }
    button {
      background: #3a6ea5; color: white; border: none;
      padding: 14px 32px; font-size: 1.1rem;
      border-radius: 6px; cursor: pointer; transition: background 0.2s;
    }
    button:hover:not(:disabled) { background: #2d5585; }
    button:disabled { background: #9ab3cc; cursor: not-allowed; }
    #status { margin-top: 16px; color: #888; font-style: italic; min-height: 24px; }
    #article-container {
      margin-top: 40px; max-width: 720px; width: 100%;
      background: white; border-radius: 10px; padding: 40px;
      box-shadow: 0 2px 12px rgba(0,0,0,0.08); display: none;
    }
    #article-container.visible { display: block; }
    #article-title { font-size: 1.6rem; font-weight: bold; color: #1a1a1a; margin-bottom: 20px; line-height: 1.3; }
    #article-body { color: #333; line-height: 1.8; font-size: 1.05rem; }
    #article-body p { margin-bottom: 16px; }
  </style>
</head>
<body>
  <h1>Article Generator</h1>
  <p class="subtitle">Powered by Claude AI</p>
  <button id="generate-btn" onclick="generateArticle()">Generate Article</button>
  <div id="status"></div>
  <div id="article-container">
    <div id="article-title"></div>
    <div id="article-body"></div>
  </div>
  <script>
    async function generateArticle() {
      const btn = document.getElementById('generate-btn');
      const status = document.getElementById('status');
      const container = document.getElementById('article-container');
      btn.disabled = true;
      status.textContent = 'Generating article\u2026';
      try {
        const res = await fetch('/generate', { method: 'POST' });
        if (!res.ok) throw new Error('Server error: ' + res.status);
        const data = await res.json();
        const lines = data.article.split('\n');
        const titleLine = lines.find(l => l.startsWith('# ')) || '';
        const title = titleLine.replace(/^# /, '').trim();
        const body = lines.filter(l => !l.startsWith('# ')).join('\n').trim();
        document.getElementById('article-title').textContent = title || 'Article';
        document.getElementById('article-body').innerHTML =
          body.split(/\n\n+/).map(p => '<p>' + p.trim() + '</p>').join('');
        container.classList.add('visible');
        status.textContent = '';
      } catch (err) {
        status.textContent = 'Error: ' + err.message;
      } finally {
        btn.disabled = false;
      }
    }
  </script>
</body>
</html>)html";

// ── main ─────────────────────────────────────────────────────────────────────

int main() {
    const char *api_key_env = std::getenv("ANTHROPIC_API_KEY");
    if (!api_key_env || std::string(api_key_env).empty()) {
        fprintf(stderr, "Error: ANTHROPIC_API_KEY environment variable not set.\n");
        return 1;
    }
    std::string api_key = api_key_env;

    curl_global_init(CURL_GLOBAL_DEFAULT);

    httplib::Server svr;

    svr.Get("/", [](const httplib::Request &, httplib::Response &res) {
        res.set_content(HTML, "text/html");
    });

    svr.Post("/generate", [&api_key](const httplib::Request &,
                                     httplib::Response &res) {
        int idx = topic_index.fetch_add(1);
        const std::string &topic = TOPICS[idx % TOPICS.size()];

        std::string article = call_claude(api_key, topic);
        if (article.empty()) {
            res.status = 500;
            res.set_content(R"({"error":"Failed to generate article"})",
                            "application/json");
            return;
        }

        json resp = {{"article", article}, {"topic", topic}};
        res.set_content(resp.dump(), "application/json");
    });

    printf("Article Generator running at http://localhost:5000\n");
    svr.listen("0.0.0.0", 5000);

    curl_global_cleanup();
    return 0;
}
