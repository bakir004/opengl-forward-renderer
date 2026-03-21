$commitMsgFile = $args[0]
$commitMsg = Get-Content $commitMsgFile -Raw

$types = "feat|fix|docs|style|refactor|perf|test|build|ci|chore|hotfix"

$pattern = "^($types)(\([a-z0-9-]+\))?: .+"

if ($commitMsg -notmatch $pattern) {
    Write-Host "" -ForegroundColor Red
    Write-Host "[ERROR] Invalid commit message format!" -ForegroundColor Red
    Write-Host "-------------------------------------------------------"
    Write-Host "Expected format: " -NoNewline; Write-Host "type(scope): description" -ForegroundColor Green
    Write-Host "Example:         " -NoNewline; Write-Host "feat(renderer): add shadow mapping" -ForegroundColor Green
    Write-Host "-------------------------------------------------------"
    Write-Host "Available keywords (types):" -Style Bold
    
    $types.Split('|') | ForEach-Object { Write-Host "  - $_" }
    
    Write-Host "-------------------------------------------------------"
    Write-Host "Note: Scope is optional but must be in parentheses if used."
    exit 1
}

exit 0
