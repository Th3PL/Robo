@echo off
chcp 65001 >nul
title Painel de coleta do carrinho
cd /d "%~dp0"
where node >nul 2>nul
if errorlevel 1 (
  echo O Node.js nao esta instalado. Instale a versao LTS em https://nodejs.org e abra este atalho de novo.
  pause
  exit /b 1
)
node dados\painel.js
pause
