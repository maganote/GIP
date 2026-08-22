import os
import anthropic
from flask import Flask, jsonify, send_from_directory

app = Flask(__name__, static_folder=".")

client = anthropic.Anthropic(api_key=os.environ.get("ANTHROPIC_API_KEY"))

TOPICS = [
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
]

_topic_index = 0


@app.route("/generate", methods=["POST"])
def generate():
    global _topic_index
    topic = TOPICS[_topic_index % len(TOPICS)]
    _topic_index += 1

    with client.messages.stream(
        model="claude-opus-4-6",
        max_tokens=1024,
        messages=[
            {
                "role": "user",
                "content": (
                    f"Write a short, engaging article (3-4 paragraphs) about: {topic}. "
                    "Include a catchy title at the top prefixed with '# '. "
                    "Keep it informative and accessible."
                ),
            }
        ],
    ) as stream:
        text = stream.get_final_message().content[0].text

    return jsonify({"article": text, "topic": topic})


@app.route("/")
def index():
    return send_from_directory(".", "index.html")


if __name__ == "__main__":
    app.run(debug=True, port=5000)
