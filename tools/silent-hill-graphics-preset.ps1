# Silent Hill: The Short Message (PPSA10112) - cumulative graphics fixes.
# Run from any directory after building/installing the experiment branch.
# This is a bundled activation of already-ported fixes, not proof of a visual cure.
$ErrorActionPreference = 'Stop'
Set-Location (Resolve-Path (Join-Path $PSScriptRoot '..'))
$env:KYTY_CB_SHADER_MASK_EXPORTS = '1'
$env:KYTY_CLEAR_REGISTER_WIDE = '1'
$env:KYTY_IMAGE_GPU_KEEP = '1'
$env:KYTY_IMAGE_CONTAINS = '1'
$env:KYTY_DEPTH_EXTENT_RECREATE = '1'
$env:KYTY_DCC_CLEAR_KEY_10 = '1'
$env:KYTY_VULKAN_VALIDATION = '1'
Remove-Item Env:KYTY_MESH_LAYER_FORCE_ZERO -ErrorAction SilentlyContinue
Write-Host 'Silent Hill cumulative graphics preset enabled (shader routing, wide clear, image preservation, depth growth).'
& '.\_Build\windows\install\kyty_launcher.exe'
