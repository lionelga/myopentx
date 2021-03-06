/*
 * Copyright (C) OpenTX
 *
 * Based on code named
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "appdebugmessagehandler.h"

AppDebugMessageHandler::AppDebugMessageHandler(QObject * parent) :
  QObject(parent),
  m_appDebugOutputLevel(APP_DBG_HANDLER_DEFAULT_LEVEL),
  m_showSourcePath(APP_DBG_HANDLER_SHOW_SRC_PATH),
  m_showFunctionDeclarations(APP_DBG_HANDLER_SHOW_FUNCTION_DECL)
{
  m_srcPathFilter = QRegularExpression("^" APP_DBG_HANDLER_SRC_PATH "[\\\\\\/](.*?)$", QRegularExpression::InvertedGreedinessOption);
  m_functionFilter = QRegularExpression("^.+?(\\w+\\().+$");
}

AppDebugMessageHandler * AppDebugMessageHandler::instance()
{
  static AppDebugMessageHandler * _instance = NULL;
  if(_instance == NULL && !qApp->closingDown())
    _instance = new AppDebugMessageHandler(qApp);  // Ensure this object is cleaned up when the QApplication exits.
  return _instance;
}

void AppDebugMessageHandler::setAppDebugOutputLevel(const quint8 & appDebugOutputLevel)
{
  m_appDebugOutputLevel = appDebugOutputLevel;
}

void AppDebugMessageHandler::setShowSourcePath(bool showSourcePath)
{
  m_showSourcePath = showSourcePath;
}

void AppDebugMessageHandler::setShowFunctionDeclarations(bool showFunctionDeclarations)
{
  m_showFunctionDeclarations = showFunctionDeclarations;
}

void AppDebugMessageHandler::installAppMessageHandler()
{
#if APP_DBG_HANDLER_ENABLE
  m_defaultHandler = qInstallMessageHandler(g_appDebugMessageHandler);
#endif
}

void AppDebugMessageHandler::messageHandler(QtMsgType type, const QMessageLogContext & context, const QString & msg)
{
  static const char symbols[] = { 'D', 'W', 'E', 'X', 'I' };  // correspond to QtMsgType enum
  // normalize types, QtDebugMsg stays 0, QtInfoMsg becomes 1, rest are QtMsgType + 1
  quint8 lvl = type;
  if (type == QtInfoMsg)
    lvl = 1;
  else if (type > QtDebugMsg)
    ++lvl;

  if (lvl < m_appDebugOutputLevel && type != QtFatalMsg)
    return;

  QString msgPattern = QString("[%1] ").arg(symbols[type]);

  if (m_showSourcePath) {
    QString file = QString("%1::").arg(context.file);
    file.replace(m_srcPathFilter, "\\1");
    msgPattern.append(file);
  }
  if (m_showFunctionDeclarations)
    msgPattern.append(context.function);
  else
    msgPattern.append("%{function}()");

  msgPattern.append(":%{line} -%{if-category} [%{category}]%{endif} %{message}");

  if (type == QtFatalMsg)
    msgPattern.append("\nBACKTRACE:\n%{backtrace depth=12 separator=\"\n\"}");

  qSetMessagePattern(msgPattern);

  if (!m_defaultHandler || receivers(SIGNAL(messageOutput(quint8, const QString &, const QMessageLogContext &)))) {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 4, 0))
    msgPattern = qFormatLogMessage(type, context, msg);
#else
    msgPattern.replace("%{line}", QString::number(context.line));
    msgPattern.replace("%{if-category} [%{category}]%{endif}", QString(context.category));
    msgPattern.replace("%{message}", msg);
    if (!m_showFunctionDeclarations) {
      QString func = context.function;
      msgPattern.replace(QString("%{function}"), func.replace(m_functionFilter, "\\1)"));
    }
#endif

    emit messageOutput(lvl, msgPattern, context);
  }

  // if (QThread::currentThread() == qApp->thread())  // gui thread

  if (m_defaultHandler) {
    m_defaultHandler(type, context, msg);
  }
  else {
    fprintf(stderr, "%s", qPrintable(msgPattern));
    if (type == QtFatalMsg)
      abort();
  }
}

// Message handler which is installed using qInstallMessageHandler. This needs to be global.
void g_appDebugMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
  if (AppDebugMessageHandler::instance())
    AppDebugMessageHandler::instance()->messageHandler(type, context, msg);
}
