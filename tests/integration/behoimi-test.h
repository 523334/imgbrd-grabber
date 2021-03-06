#ifndef BEHOIMI_TEST_H
#define BEHOIMI_TEST_H

#include "integration-test-suite.h"


class BehoimiTest : public IntegrationTestSuite
{
	Q_OBJECT

	private slots:
		void testHtml();
		void testXml();
		void testJson();
		void testPageTags();
};

#endif // BEHOIMI_TEST_H
