function makeprg_release()
  vim.o.makeprg = 'make release'
  vim.cmd("make")
end

function makeprg_debug()
  vim.o.makeprg = 'make debug'
  vim.cmd("make")
end

function makeprg_release_optimized()
  vim.o.makeprg = 'make release-optimize'
  vim.cmd("make")
end

--vim.keymap.set('i', '<Tab>', '<Tab>', { noremap = true, silent = true })
-- set noexpandtab but in lua
vim.opt_local.expandtab = false
vim.opt_local.shiftwidth = 2
vim.opt_local.tabstop = 2
