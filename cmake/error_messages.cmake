# =============================================================================
# Module: scrapers (ProcFs files scrapers)
# =============================================================================
# Error message template for scraper errors
set(SCRAPERS_ERROR_TEMPLATE [[
├── Error Message: {}
├── Scraper:       {}
├── Source file:   {}
└── Position:      {}]])

# Error messages for specific scraper error codes
set(MSG_ERR_SCRAPER_INVALID_SYNTAX      "Invalid syntax")
set(MSG_ERR_SCRAPER_FILE_NOT_FOUND      "File not found or failed to open")
set(MSG_ERR_SCRAPER_FILE_READ_FAILED    "Failed to read file")
set(MSG_ERR_SCRAPER_FILE_TOO_LARGE      "File too large")

# =============================================================================
# Module: sys_io (Filesystem I/O operations)
# =============================================================================
# Error message template for filesystem errors
set(FS_ERROR_TEMPLATE [[
├── Error Message: {}
├── OS Context:    {}
├── Path:          {}
└── Offset:        {}]])

# Error messages for specific filesystem error codes
set(MSG_ERR_FS_NOT_FOUND                "Not found")
set(MSG_ERR_FS_ACCESS_DENIED            "Access denied")
set(MSG_ERR_FS_DESCRIPTOR_LIMIT         "Descriptor limit reached")
set(MSG_ERR_FS_READ_FAILURE             "Read failure")
set(MSG_ERR_FS_SIZE_LIMIT               "Size limit exceeded")
set(MSG_ERR_FS_INVALID_CONTEXT          "Invalid file context")
