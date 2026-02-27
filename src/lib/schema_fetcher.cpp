#include <xb/schema_fetcher.hpp>

#include <xb/expat_reader.hpp>
#include <xb/rng_compact_parser.hpp>
#include <xb/rng_parser.hpp>
#include <xb/schema_parser.hpp>

#include <cctype>
#include <deque>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace xb {

  namespace {

    bool
    has_extension(const std::string& url, const std::string& ext) {
      if (url.size() < ext.size()) return false;
      auto suffix = url.substr(url.size() - ext.size());
      for (std::size_t i = 0; i < ext.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(suffix[i])) !=
            std::tolower(static_cast<unsigned char>(ext[i])))
          return false;
      }
      return true;
    }

    // Collect external references from a RELAX NG pattern tree
    void
    collect_rng_refs(const rng::pattern& p, std::vector<std::string>& refs) {
      if (p.holds<rng::external_ref_pattern>()) {
        auto& href = p.get<rng::external_ref_pattern>().href;
        if (!href.empty()) refs.push_back(href);
      } else if (p.holds<rng::grammar_pattern>()) {
        auto& g = p.get<rng::grammar_pattern>();
        if (g.start) collect_rng_refs(*g.start, refs);
        for (auto& d : g.defines) {
          if (d.body) collect_rng_refs(*d.body, refs);
        }
        for (auto& inc : g.includes) {
          if (!inc.href.empty()) refs.push_back(inc.href);
        }
      } else if (p.holds<rng::element_pattern>()) {
        auto& e = p.get<rng::element_pattern>();
        if (e.content) collect_rng_refs(*e.content, refs);
      } else if (p.holds<rng::attribute_pattern>()) {
        auto& a = p.get<rng::attribute_pattern>();
        if (a.content) collect_rng_refs(*a.content, refs);
      } else if (p.holds<rng::group_pattern>()) {
        auto& g = p.get<rng::group_pattern>();
        if (g.left) collect_rng_refs(*g.left, refs);
        if (g.right) collect_rng_refs(*g.right, refs);
      } else if (p.holds<rng::interleave_pattern>()) {
        auto& il = p.get<rng::interleave_pattern>();
        if (il.left) collect_rng_refs(*il.left, refs);
        if (il.right) collect_rng_refs(*il.right, refs);
      } else if (p.holds<rng::choice_pattern>()) {
        auto& ch = p.get<rng::choice_pattern>();
        if (ch.left) collect_rng_refs(*ch.left, refs);
        if (ch.right) collect_rng_refs(*ch.right, refs);
      } else if (p.holds<rng::one_or_more_pattern>()) {
        auto& om = p.get<rng::one_or_more_pattern>();
        if (om.content) collect_rng_refs(*om.content, refs);
      } else if (p.holds<rng::zero_or_more_pattern>()) {
        auto& zm = p.get<rng::zero_or_more_pattern>();
        if (zm.content) collect_rng_refs(*zm.content, refs);
      } else if (p.holds<rng::optional_pattern>()) {
        auto& op = p.get<rng::optional_pattern>();
        if (op.content) collect_rng_refs(*op.content, refs);
      } else if (p.holds<rng::mixed_pattern>()) {
        auto& mp = p.get<rng::mixed_pattern>();
        if (mp.content) collect_rng_refs(*mp.content, refs);
      } else if (p.holds<rng::list_pattern>()) {
        auto& lp = p.get<rng::list_pattern>();
        if (lp.content) collect_rng_refs(*lp.content, refs);
      }
    }

    bool
    is_absolute_url(const std::string& url) {
      return url.starts_with("http://") || url.starts_with("https://") ||
             url.starts_with("/");
    }

    // Split a URL into (authority_prefix, path) where authority_prefix is
    // "scheme://host" for HTTP URLs, or empty for local paths.
    std::pair<std::string, std::string>
    split_authority(const std::string& url) {
      for (auto scheme : {"https://", "http://"}) {
        if (url.starts_with(scheme)) {
          auto path_start = url.find('/', std::string(scheme).size());
          if (path_start == std::string::npos) return {url, "/"};
          return {url.substr(0, path_start), url.substr(path_start)};
        }
      }
      return {"", url};
    }

    // Return the parent directory of a path (everything up to last '/').
    std::string
    parent_path(const std::string& path) {
      auto pos = path.rfind('/');
      if (pos == std::string::npos) return "";
      return path.substr(0, pos + 1);
    }

    // Normalize a path: resolve "." and ".." components.
    std::string
    normalize_path(const std::string& path) {
      std::vector<std::string> parts;
      std::string component;
      for (std::size_t i = 0; i < path.size(); ++i) {
        if (path[i] == '/') {
          if (component == "..") {
            if (!parts.empty()) parts.pop_back();
          } else if (component != "." && !component.empty()) {
            parts.push_back(component);
          }
          component.clear();
        } else {
          component += path[i];
        }
      }
      if (component == "..") {
        if (!parts.empty()) parts.pop_back();
      } else if (component != "." && !component.empty()) {
        parts.push_back(component);
      }

      std::string result;
      if (!path.empty() && path[0] == '/') result = "/";
      for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) result += "/";
        result += parts[i];
      }
      return result;
    }

  } // namespace

  std::string
  resolve_url(const std::string& base_url, const std::string& relative) {
    if (is_absolute_url(relative)) return relative;

    auto [authority, base_path] = split_authority(base_url);
    auto parent = parent_path(base_path);
    auto combined = parent + relative;
    auto normalized = normalize_path(combined);

    return authority + normalized;
  }

  std::vector<fetched_schema>
  crawl_schemas(const std::string& root_url, const transport_fn& transport,
                const fetch_options& opts) {
    std::vector<fetched_schema> results;
    std::deque<std::string> queue;
    std::unordered_set<std::string> visited;

    queue.push_back(root_url);

    while (!queue.empty()) {
      auto url = std::move(queue.front());
      queue.pop_front();

      if (visited.count(url)) continue;
      visited.insert(url);

      std::string content;
      try {
        content = transport(url);
      } catch (const std::exception& e) {
        if (opts.fail_fast) throw;
        std::cerr << "xb fetch: warning: " << e.what() << "\n";
        continue;
      }

      results.push_back({url, content});

      // Parse to extract import/include references
      try {
        if (has_extension(url, ".rnc")) {
          rng_compact_parser parser;
          auto pattern = parser.parse(content);
          std::vector<std::string> refs;
          collect_rng_refs(pattern, refs);
          for (auto& href : refs) {
            auto resolved = resolve_url(url, href);
            if (!visited.count(resolved)) queue.push_back(std::move(resolved));
          }
        } else if (has_extension(url, ".rng")) {
          expat_reader reader(content);
          rng_xml_parser parser;
          auto pattern = parser.parse(reader);
          std::vector<std::string> refs;
          collect_rng_refs(pattern, refs);
          for (auto& href : refs) {
            auto resolved = resolve_url(url, href);
            if (!visited.count(resolved)) queue.push_back(std::move(resolved));
          }
        } else {
          expat_reader reader(content);
          schema_parser parser;
          auto s = parser.parse(reader);

          for (const auto& imp : s.imports()) {
            if (imp.schema_location.empty()) continue;
            auto resolved = resolve_url(url, imp.schema_location);
            if (!visited.count(resolved)) queue.push_back(std::move(resolved));
          }
          for (const auto& inc : s.includes()) {
            if (inc.schema_location.empty()) continue;
            auto resolved = resolve_url(url, inc.schema_location);
            if (!visited.count(resolved)) queue.push_back(std::move(resolved));
          }
        }
      } catch (const std::exception&) {
        // Parse failed — keep the content, skip transitive deps
      }
    }

    return results;
  }

  namespace {

    // Extract the path portion of a URL (after scheme://authority).
    std::string
    extract_path(const std::string& url) {
      auto [authority, path] = split_authority(url);
      return path;
    }

    // Find the longest common directory prefix across paths (at '/'
    // boundaries).
    std::string
    common_dir_prefix(const std::vector<std::string>& paths) {
      if (paths.empty()) return "";
      if (paths.size() == 1) {
        auto pos = paths[0].rfind('/');
        if (pos == std::string::npos) return "";
        return paths[0].substr(0, pos + 1);
      }

      auto first = paths[0];
      std::size_t prefix_len = first.size();

      for (std::size_t i = 1; i < paths.size(); ++i) {
        std::size_t j = 0;
        auto limit = std::min(prefix_len, paths[i].size());
        while (j < limit && first[j] == paths[i][j])
          ++j;
        prefix_len = j;
      }

      // Truncate to last '/' boundary
      auto prefix = first.substr(0, prefix_len);
      auto pos = prefix.rfind('/');
      if (pos == std::string::npos) return "";
      return prefix.substr(0, pos + 1);
    }

  } // namespace

  std::vector<schema_location_entry>
  compute_local_paths(const std::vector<fetched_schema>& schemas) {
    std::vector<std::string> paths;
    paths.reserve(schemas.size());
    for (const auto& s : schemas)
      paths.push_back(extract_path(s.source_url));

    auto prefix = common_dir_prefix(paths);
    auto prefix_len = prefix.size();

    std::vector<schema_location_entry> entries;
    entries.reserve(schemas.size());
    for (std::size_t i = 0; i < schemas.size(); ++i) {
      schema_location_entry entry;
      entry.url = schemas[i].source_url;
      entry.local_path = paths[i].substr(prefix_len);
      entry.size = schemas[i].content.size();
      entries.push_back(std::move(entry));
    }
    return entries;
  }

  namespace {

    std::string
    json_escape(const std::string& s) {
      std::string out;
      out.reserve(s.size());
      for (char c : s) {
        switch (c) {
          case '"':
            out += "\\\"";
            break;
          case '\\':
            out += "\\\\";
            break;
          case '\n':
            out += "\\n";
            break;
          case '\r':
            out += "\\r";
            break;
          case '\t':
            out += "\\t";
            break;
          default:
            out += c;
            break;
        }
      }
      return out;
    }

  } // namespace

  void
  write_manifest(const std::string& path, const fetch_manifest& manifest) {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("cannot write manifest: " + path);

    out << "{\n";
    out << "  \"root\": \"" << json_escape(manifest.root_url) << "\",\n";
    out << "  \"fetched\": \"" << json_escape(manifest.fetched_at) << "\",\n";
    out << "  \"schemas\": [\n";

    for (std::size_t i = 0; i < manifest.schemas.size(); ++i) {
      const auto& s = manifest.schemas[i];
      out << "    {\"url\": \"" << json_escape(s.url) << "\", "
          << "\"path\": \"" << json_escape(s.local_path) << "\", "
          << "\"size\": " << s.size << "}";
      if (i + 1 < manifest.schemas.size()) out << ",";
      out << "\n";
    }

    out << "  ]\n";
    out << "}\n";
  }

} // namespace xb
