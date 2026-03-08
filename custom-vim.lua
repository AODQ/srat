function makeprg_release()
  vim.o.makeprg = 'ninja install -C ./build'
  vim.cmd("make")
end

--vim.keymap.set('i', '<Tab>', '<Tab>', { noremap = true, silent = true })
-- set noexpandtab but in lua
vim.opt_local.expandtab = false
vim.opt_local.shiftwidth = 2
vim.opt_local.tabstop = 2
