import test from 'node:test';
import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {readFileSync,mkdtempSync,writeFileSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';

test('fixture rejects the wrong scheduled identity before report or build', {skip:process.platform !== 'win32'},()=>{
 const root=mkdtempSync(join(tmpdir(),'matrix-fixture-policy-'));
 try {
  const script=readFileSync(new URL('../Supervised-Fixture.ps1',import.meta.url),'utf8')
   .replace('__AGENT__','S-1-5-21-1-2-3-9999').replace('__BROKER__',root).replace("'__HASHES__'=''","'missing.ps1'='bad'");
  const file=join(root,'entry.ps1');writeFileSync(file,script);
  assert.throws(()=>execFileSync('pwsh.exe',['-NoProfile','-NonInteractive','-File',file],{encoding:'utf8',windowsHide:true,stdio:'pipe'}),e=>/actual non-admin Agent token/.test(e.stderr));
 } finally {rmSync(root,{recursive:true,force:true});}
});


test('fixture integrity and cleanup gates reject changed source and retained lease', {skip:process.platform !== 'win32'},()=>{
 const path=new URL('../Supervised-Fixture.ps1',import.meta.url).pathname.replace(/^\//,'');
 const command=`$ast=[Management.Automation.Language.Parser]::ParseFile('${path}',[ref]$null,[ref]$null);$fn=$ast.Find({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Assert-FixtureClean'},$true);Invoke-Expression $fn.Extent.Text;$d=Join-Path $env:TEMP ([guid]::NewGuid().ToString('N'));$null=New-Item -ItemType Directory $d;try{$f=Join-Path $d 'source';Set-Content $f 'original';$h=@{source=(Get-FileHash $f).Hash};$j=Join-Path $d 'lease';Assert-FixtureClean $d $h $j;Set-Content $f 'changed';$denied=$false;try{Assert-FixtureClean $d $h $j}catch{$denied=$true};if(-not $denied){throw 'Tampering accepted'};$h.source=(Get-FileHash $f).Hash;Set-Content $j 'retained';$denied=$false;try{Assert-FixtureClean $d $h $j}catch{$denied=$true};if(-not $denied){throw 'Lease accepted'};'PASS'}finally{Remove-Item -LiteralPath $d -Recurse -Force}`;
 assert.match(execFileSync('pwsh.exe',['-NoProfile','-NonInteractive','-Command',command],{encoding:'utf8',windowsHide:true}),/PASS/);
});

test('fixture permission gate refuses writable trusted scripts without altering contents', {skip:process.platform !== 'win32'},()=>{
 const path=new URL('../Supervised-Fixture.ps1',import.meta.url).pathname.replace(/^\//,'');
 const command=`$ast=[Management.Automation.Language.Parser]::ParseFile('${path}',[ref]$null,[ref]$null);$fn=$ast.Find({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Assert-FixtureReadOnly'},$true);Invoke-Expression $fn.Extent.Text;$f=Join-Path $env:TEMP ([guid]::NewGuid().ToString('N'));try{Set-Content $f 'keep';$hash=(Get-FileHash $f).Hash;$rejected=$false;try{Assert-FixtureReadOnly @($f)}catch{if($_.Exception.Message -ne 'Agent can write a trusted entry point'){throw};$rejected=$true};if(-not $rejected -or (Get-FileHash $f).Hash -ne $hash){throw 'Writable script accepted or changed'};'PASS'}finally{Remove-Item -LiteralPath $f -Force}`;
 assert.match(execFileSync('pwsh.exe',['-NoProfile','-NonInteractive','-Command',command],{encoding:'utf8',windowsHide:true}),/PASS/);
});
