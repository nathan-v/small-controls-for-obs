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

#pragma once

#include <QDebug>
#include <QString>

#include <functional>
#include <vector>

/*
 * Minimal test harness on top of Qt Widgets. QtTest is not part of the Qt
 * that obs-deps ships, so this keeps the suite buildable wherever the plugin
 * builds. A test is a function registered with TEST(name); assertions throw
 * TestFailure, which main() reports and counts.
 */

struct TestFailure {
	QString message;
};

struct TestCase {
	const char *name;
	std::function<void()> fn;
};

std::vector<TestCase> &testRegistry();

struct TestRegistrar {
	TestRegistrar(const char *name, std::function<void()> fn) { testRegistry().push_back({name, std::move(fn)}); }
};

template<typename T> QString testDisplay(const T &value)
{
	QString out;
	QDebug(&out).nospace() << value;
	return out.trimmed();
}

[[noreturn]] inline void testFail(const char *file, int line, const QString &what)
{
	throw TestFailure{QStringLiteral("%1:%2: %3").arg(QString::fromUtf8(file)).arg(line).arg(what)};
}

#define TEST(name)                                                            \
	static void test_##name();                                            \
	static TestRegistrar registrar_##name(#name, test_##name);            \
	static void test_##name()

#define CHECK(cond)                                                           \
	do {                                                                  \
		if (!(cond))                                                  \
			testFail(__FILE__, __LINE__, QStringLiteral("CHECK(" #cond ") failed")); \
	} while (0)

#define CHECK_MSG(cond, msg)                                                  \
	do {                                                                  \
		if (!(cond))                                                  \
			testFail(__FILE__, __LINE__, QStringLiteral("CHECK(" #cond ") failed: %1").arg(msg)); \
	} while (0)

#define CHECK_EQ(actual, expected)                                            \
	do {                                                                  \
		const auto &a_ = (actual);                                    \
		const auto &e_ = (expected);                                  \
		if (!(a_ == e_))                                              \
			testFail(__FILE__, __LINE__,                          \
				 QStringLiteral(#actual " == " #expected " failed: got %1, expected %2") \
					 .arg(testDisplay(a_), testDisplay(e_)));  \
	} while (0)
