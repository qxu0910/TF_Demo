package factory;

import com.google.gson.*;

/** L8 输入契约。显式检查类型，避免 JSON 的数字/布尔值被隐式转换成字符串。 */
public record ChatRequest(String model, String prompt) {
    public static ChatRequest parse(JsonElement value) {
        if (value == null || !value.isJsonObject()) throw bad("请求必须是 JSON 对象");
        JsonObject body = value.getAsJsonObject();
        for (String key : body.keySet()) {
            if (!java.util.Set.of("model", "messages", "stream").contains(key))
                throw bad("暂不支持参数：" + key);
        }
        String model = string(body.get("model"), "model");
        if (!model.equals("factory-mock-v1")) throw bad("model 必须为 factory-mock-v1");
        if (body.has("stream")) {
            JsonElement stream = body.get("stream");
            if (!stream.isJsonPrimitive() || !stream.getAsJsonPrimitive().isBoolean() || stream.getAsBoolean())
                throw bad("当前仅支持 stream: false");
        }
        JsonElement raw = body.get("messages");
        if (raw == null || !raw.isJsonArray() || raw.getAsJsonArray().size() != 1)
            throw bad("当前仅支持一条 user 消息，不支持多轮上下文");
        JsonElement first = raw.getAsJsonArray().get(0);
        if (!first.isJsonObject()) throw bad("messages[0] 必须是对象");
        JsonObject message = first.getAsJsonObject();
        if (!message.keySet().equals(java.util.Set.of("role", "content"))) throw bad("消息仅支持 role 和 content");
        if (!string(message.get("role"), "role").equals("user")) throw bad("role 必须为 user");
        String prompt = string(message.get("content"), "content");
        if (prompt.isBlank() || prompt.length() > 2000) throw bad("content 必须为 1–2000 个 UTF-16 字符且不能全为空白");
        return new ChatRequest(model, prompt);
    }

    private static String string(JsonElement value, String field) {
        if (value == null || !value.isJsonPrimitive() || !value.getAsJsonPrimitive().isString())
            throw bad(field + " 必须为字符串");
        return value.getAsString();
    }

    private static IllegalArgumentException bad(String message) { return new IllegalArgumentException(message); }
}
