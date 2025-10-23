-- Clangd configuration for heavy template metaprogramming
-- Add this to your nvim LSP config (e.g., ~/.config/nvim/init.lua or lsp config file)
--
-- This config tries to prevent crashes while keeping full functionality

require('lspconfig').clangd.setup {
  cmd = {
    "clangd",
    -- Memory and resource management to reduce crashes
    "--pch-storage=memory",
    "--header-insertion=iwyu",
    "-j=8", -- Adjust based on your CPU cores

    -- Background indexing helps with stability
    "--background-index",

    -- Completion settings
    "--completion-style=detailed",
    "--all-scopes-completion",
    "--function-arg-placeholders",

    -- Enable clang-tidy (configured in .clangd file)
    "--clang-tidy",
  },

  init_options = {
    clangdFileStatus = true,
  },

  on_attach = function(client, bufnr)
    -- Your other on_attach config here
    -- Enable standard LSP keybindings, etc.
  end,
}
