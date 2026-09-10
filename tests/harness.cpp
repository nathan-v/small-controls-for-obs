/*
Small Controls for OBS
Copyright (C) 2026 Nathan V

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "harness.hpp"

#include <QApplication>

#include <cstdio>

std::vector<TestCase> &testRegistry()
{
	static std::vector<TestCase> registry;
	return registry;
}

/* The offscreen platform warns about size hints on every show(); drop it. */
static void quietMessages(QtMsgType type, const QMessageLogContext &, const QString &message)
{
	if (message.contains(QStringLiteral("propagateSizeHints")))
		return;
	fprintf(stderr, "%s%s\n", type == QtWarningMsg ? "warning: " : "", qPrintable(message));
}

int main(int argc, char **argv)
{
	/* headless by default; override with QT_QPA_PLATFORM to watch it run */
	if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
		qputenv("QT_QPA_PLATFORM", "offscreen");
	qInstallMessageHandler(quietMessages);

	QApplication app(argc, argv);

	int failed = 0;
	for (const TestCase &test : testRegistry()) {
		try {
			test.fn();
			printf("PASS %s\n", test.name);
		} catch (const TestFailure &failure) {
			failed++;
			printf("FAIL %s\n     %s\n", test.name, qPrintable(failure.message));
		}
		fflush(stdout);
	}

	printf("%zu tests, %d failed\n", testRegistry().size(), failed);
	return failed ? 1 : 0;
}
