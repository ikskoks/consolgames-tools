#include <QApplication>
#include "PatchWizard.h"
#include <QIcon>
#include <QLocale>
#include <QFile>
#include <QTranslator>

QString loadTextFile(const QString& filename, const QString& defaultText = "")
{
	QFile file(filename);
	if (!file.open(QIODevice::ReadOnly))
	{
		return defaultText;
	}
	return file.readAll();
}

int main(int argc, char* argv[])
{
	QLocale::setDefault(QLocale(QLocale::Russian, QLocale::RussianFederation));

	QApplication application(argc, argv);

	QTranslator *translator = new QTranslator(&application); 
	if (translator->load(":/qt_ru.qm")) 
	{
		application.installTranslator(translator);
	}
	application.setStyleSheet(loadTextFile(":/Patcher.css"));
	application.setWindowIcon(QIcon(":/SamusHelmet.png"));
	application.setApplicationName("Metroid Prime 3 Translation Patch");
	application.setApplicationVersion(loadTextFile(":/version", "1.0"));
	application.setOrganizationName("Consolgames");
	application.setOrganizationDomain("consolgames.ru");

#if !defined(PRODUCTION)
	const bool checkingEnabled = !application.arguments().contains("--no-check");
#else
	const bool checkingEnabled = application.arguments().contains("--check");
#endif

	PatchWizard wizard;
	wizard.setCheckingEnabled(checkingEnabled);
	wizard.show();

	return application.exec();
}
