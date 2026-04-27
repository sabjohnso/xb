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

    /// URL schemes the fetcher is permitted to retrieve. URLs whose
    /// scheme is not in this set raise @c std::runtime_error before the
    /// transport is invoked.  Defaults to ={"https"}= so that an
    /// attacker-supplied @c schemaLocation cannot redirect the fetcher
    /// to @c file://, @c gopher://, etc.
    std::vector<std::string> scheme_allowlist = {"https"};

    /// When @c true, absolute local paths (URLs that begin with @c "/")
    /// are accepted in addition to the schemes in
    /// @ref scheme_allowlist.  Operators that intentionally pass a
    /// local schema to the CLI set this; libraries handling untrusted
    /// input should leave it @c false.
    bool allow_local_paths = false;
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
