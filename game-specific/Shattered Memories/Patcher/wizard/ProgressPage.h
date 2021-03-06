#pragma once
#include "Page.h"
#include "ui_ProgressPage.h"
#include <PatcherController.h>
#include <QThread>
#include <QHash>

class ProgressPage : public Page<Ui_ProgressPage>
{
	Q_OBJECT

public:
	ProgressPage();

	void startPatching();
	void finalizePatching();

	virtual int nextId() const override;
	virtual bool isComplete() const override;
	virtual void initializePage() override;

	Q_SLOT void onPatchingCompleted();
	Q_SLOT void onStepStarted(const QByteArray& step);
	Q_SLOT void onStepCompleted(const QByteArray& step);
	Q_SLOT void onPatchingFailed(const QByteArray& step, int errorCode, const QString& errorData);
	Q_SLOT void onPatchingCanceled(const QByteArray& step);
	Q_SLOT void onProgress(int, const QString& message);
	Q_SLOT void onCancelPressed();

private:
	enum State
	{
		InProgress,
		Completed,
		Failed,
		Canceled
	};

private:
	QList<QByteArray> m_steps;
	ShatteredMemories::PatcherController m_patcherController;
	State m_state;
	static QHash<QString,QString> s_locInfo;
};
