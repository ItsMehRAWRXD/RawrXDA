#include "react_server_generator.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>

namespace RawrXD {

static void write_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[ReactServerGenerator] Failed to write " << path.string() << "\n";
        return;
    }
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
}

static std::string package_json(const std::string& name, int port) {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"name\": \"" << name << "\",\n";
    oss << "  \"version\": \"1.0.0\",\n";
    oss << "  \"type\": \"module\",\n";
    oss << "  \"scripts\": {\n";
    oss << "    \"dev\": \"node server.js\",\n";
    oss << "    \"start\": \"node server.js\"\n";
    oss << "  },\n";
    oss << "  \"engines\": {\n";
    oss << "    \"node\": \">=20\"\n";
    oss << "  },\n";
    oss << "  \"rawrxd\": {\n";
    oss << "    \"port\": " << port << "\n";
    oss << "  }\n";
    oss << "}\n";
    return oss.str();
}

static std::string index_html(const std::string& title) {
    std::ostringstream oss;
    oss << "<!doctype html>\n";
    oss << "<html lang=\"en\">\n";
    oss << "  <head>\n";
    oss << "    <meta charset=\"UTF-8\" />\n";
    oss << "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" />\n";
    oss << "    <title>" << title << "</title>\n";
    oss << "    <script crossorigin src=\"https://unpkg.com/react@18/umd/react.production.min.js\"></script>\n";
    oss << "    <script crossorigin src=\"https://unpkg.com/react-dom@18/umd/react-dom.production.min.js\"></script>\n";
    oss << "  </head>\n";
    oss << "  <body>\n";
    oss << "    <div id=\"root\"></div>\n";
    oss << "    <script type=\"module\" src=\"/app.js\"></script>\n";
    oss << "  </body>\n";
    oss << "</html>\n";
    return oss.str();
}

static std::string app_js(const std::string& title) {
    std::ostringstream oss;
    oss << "const root = ReactDOM.createRoot(document.getElementById('root'));\n";
    oss << "const App = () => React.createElement('div', { style: { fontFamily: 'sans-serif', padding: '24px' } }, [\n";
    oss << "  React.createElement('h1', { key: 'h1' }, '" << title << "'),\n";
    oss << "  React.createElement('p', { key: 'p' }, 'RawrXD React server scaffold running with zero deps.'),\n";
    oss << "  React.createElement('div', { key: 'info', style: { marginTop: '12px', opacity: 0.7 } }, 'Edit app.js to customize UI.')\n";
    oss << "]);\n";
    oss << "root.render(React.createElement(App));\n";
    return oss.str();
}

static std::string server_js(int port) {
    std::ostringstream oss;
    oss << "import http from 'http';\n";
    oss << "import { readFile } from 'fs/promises';\n";
    oss << "import { extname, join } from 'path';\n\n";
    oss << "const port = process.env.PORT ? Number(process.env.PORT) : " << port << ";\n";
    oss << "const root = process.cwd();\n\n";
    oss << "const mime = {\n";
    oss << "  '.html': 'text/html',\n";
    oss << "  '.js': 'text/javascript',\n";
    oss << "  '.css': 'text/css',\n";
    oss << "};\n\n";
    oss << "const server = http.createServer(async (req, res) => {\n";
    oss << "  try {\n";
    oss << "    const url = req.url === '/' ? '/index.html' : req.url;\n";
    oss << "    const filePath = join(root, url);\n";
    oss << "    const data = await readFile(filePath);\n";
    oss << "    res.writeHead(200, { 'Content-Type': mime[extname(filePath)] || 'application/octet-stream' });\n";
    oss << "    res.end(data);\n";
    oss << "  } catch (err) {\n";
    oss << "    res.writeHead(404, { 'Content-Type': 'text/plain' });\n";
    oss << "    res.end('Not Found');\n";
    oss << "  }\n";
    oss << "});\n\n";
    oss << "server.listen(port, () => {\n";
    oss << "  console.log(`RawrXD React server running on http://localhost:${port}`);\n";
    oss << "});\n";
    return oss.str();
}

std::string ReactServerGenerator::Generate(const std::string& project_name, const ReactServerConfig& config) {
    std::filesystem::path base = config.outputDir.empty()
        ? std::filesystem::current_path() / project_name
        : std::filesystem::path(config.outputDir);

    std::filesystem::create_directories(base);
    write_file(base / "package.json", package_json(project_name, config.port));
    write_file(base / "index.html", index_html(project_name));
    write_file(base / "app.js", app_js(project_name));
    write_file(base / "server.js", server_js(config.port));

    std::ostringstream summary;
    summary << "[ReactServerGenerator] Files created:\n";
    summary << "  - package.json\n";
    summary << "  - index.html\n";
    summary << "  - app.js\n";
    summary << "  - server.js\n";
    return summary.str();
}

} // namespace RawrXD
