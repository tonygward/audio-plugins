[CmdletBinding()]
param([switch]$PlanOnly)
$ErrorActionPreference='Stop'
$common=Join-Path $PSScriptRoot 'Build-BrokerCommon.ps1'
if((Get-FileHash -LiteralPath $common).Hash -ne '__COMMON_HASH__'){throw 'Test dependency mismatch'}
. $common
$contract=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'broker.json') -Raw | ConvertFrom-Json
$task=Get-BrokerTask $contract -AllowDisabled:$PlanOnly
if($PlanOnly){@{task=$contract.taskName;enabled=$task.Enabled;fixtureOnly=$true;startsTask=$false;createsWorktree=$false;callsAgentProvider=$false} | ConvertTo-Json;return}
$identity=[Security.Principal.WindowsIdentity]::GetCurrent()
if($identity.User.Value -ne $contract.agentSid -or ([Security.Principal.WindowsPrincipal]::new($identity)).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){throw 'First test must run as the non-admin Agent'}
$c=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'worker.config.proposed.json') -Raw | ConvertFrom-Json
if($c.liveExecution -ne $false -or $c.worker.enabled -ne $false -or $c.autoMerge -ne $false -or $c.releasePublication -ne $false){throw 'Keep all autonomous switches false'}
$root=Join-Path $contract.worktreeRoot ('BROKER-TEST-'+[Guid]::NewGuid().ToString('N'))
$null=New-Item -ItemType Directory -Path (Join-Path $root 'eq'),(Join-Path $root 'build/matrix') -Force
Set-Content -LiteralPath (Join-Path $root '.git') -Value 'private fixture pointer'
Set-Content -LiteralPath (Join-Path $root 'eq/main.cpp') -Value @'
#include <windows.h>
#include <sddl.h>
#include <string>
#include <iostream>
int main() {
    HANDLE token{}; if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)) return 1;
    DWORD size{}; GetTokenInformation(token,TokenUser,nullptr,0,&size);
    std::string storage(size,'\0'); if(!GetTokenInformation(token,TokenUser,&storage[0],size,&size)) return 2;
    char* sid{}; if(!ConvertSidToStringSidA(reinterpret_cast<TOKEN_USER*>(&storage[0])->User.Sid,&sid)) return 3;
    const bool expected=std::string(sid)==EXPECTED_BUILD_SID; LocalFree(sid); CloseHandle(token);
    if(!expected) return 4;
    SID_IDENTIFIER_AUTHORITY authority=SECURITY_NT_AUTHORITY; PSID admins{}; BOOL elevated{};
    if(!AllocateAndInitializeSid(&authority,2,SECURITY_BUILTIN_DOMAIN_RID,DOMAIN_ALIAS_RID_ADMINS,0,0,0,0,0,0,&admins)) return 5;
    if(!CheckTokenMembership(nullptr,admins,&elevated)) return 6; FreeSid(admins); if(elevated) return 7;
    HANDLE source=CreateFileA(FIXTURE_SOURCE,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
    if(source!=INVALID_HANDLE_VALUE){CloseHandle(source);return 8;}
    if(GetLastError()!=ERROR_ACCESS_DENIED) return 9;
    std::cout << "Verified actual non-admin build SID and denied source write\n"; return 0;
}
'@
$cmake=@'
cmake_minimum_required(VERSION 3.22)
project(MatrixBrokerFixture LANGUAGES CXX)
if(NOT IS_DIRECTORY "${JUCE_DIR}")
  message(FATAL_ERROR "Missing JUCE source")
endif()
add_executable(EQTests main.cpp)
target_link_libraries(EQTests PRIVATE advapi32)
target_compile_definitions(EQTests PRIVATE EXPECTED_BUILD_SID="__SID__" FIXTURE_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
set_target_properties(EQTests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/EQTests_artefacts")
'@
Set-Content -LiteralPath (Join-Path $root 'eq/CMakeLists.txt') -Value $cmake.Replace('__SID__',$contract.buildSid)
$source=Join-Path $root 'eq/main.cpp';$before=(Get-FileHash -LiteralPath $source).Hash
$request=Join-Path $root 'build/matrix/request.json'
@{worktree=$root;juceDirectory=$contract.juceDirectory;plugins=@('eq');isolationTargets=@()} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $request
& (Join-Path $PSScriptRoot 'Invoke-MatrixScheduledBuild.ps1') -RequestFile $request -WorktreeRoot $contract.worktreeRoot
if($LASTEXITCODE -ne 0){throw 'First fixed build fixture failed'}
if((Get-FileHash -LiteralPath $source).Hash -ne $before -or (Test-Path -LiteralPath $contract.journalFile)){throw 'Source changed or lease remains'}
@{passed=$true;worktree=$root;actualControllerSid=$identity.User.Value;sourceUnchanged=$true;leaseClosed=$true;modelTurns=0} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'build/matrix/first-test.json')
Write-Output "PASS: fixed broker fixture only; no model turn. Evidence: $root"
