#pragma once
#include <QObject>
#include <QList>
#include <QNetworkReply>

#include "QObjectPtr.h"
#include "minecraft/auth/AccountData.h"
#include "AccountTask.h"

class AuthStep : public QObject {
    Q_OBJECT

public:
    using Ptr = shared_qobject_ptr<AuthStep>;

public:
    explicit AuthStep(AccountData *data);
    virtual ~AuthStep() noexcept;

    virtual QString describe() = 0;

public slots:
    virtual void perform() = 0;
    virtual void rehydrate() = 0;

signals:
    void finished(AccountTaskState resultingState, QString message);
    void authorizeWithBrowser(const QUrl &url);

protected:
    AccountData *m_data;
};
