#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace xb {

  using transport_fn = std::function<std::string(const std::string& url)>;

  struct fetched_schema {
    std::string source_url;
    std::string content;
  };

  struct fetch_options {
    bool fail_fast = false;
  };

  struct schema_location_entry {
    std::string url;
    std::string local_path;
    std::size_t size = 0;
  };

  struct fetch_manifest {
    std::string root_url;
    std::string fetched_at;
    std::vector<schema_location_entry> schemas;
  };

  std::string
  resolve_url(const std::string& base_url, const std::string& relative);

  std::vector<fetched_schema>
  crawl_schemas(const std::string& root_url, const transport_fn& transport,
                const fetch_options& opts = {});

  std::vector<schema_location_entry>
  compute_local_paths(const std::vector<fetched_schema>& schemas);

  void
  write_manifest(const std::string& path, const fetch_manifest& manifest);

} // namespace xb
