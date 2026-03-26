
#pragma once
#include <QObject>

#include "QObjectPtr.h"
#include "minecraft/auth/AuthStep.h"

#include <QOAuth2AuthorizationCodeFlow>
#include <QOAuthHttpServerReplyHandler>

class MSAStep : public AuthStep {
    Q_OBJECT
public:
    enum Action {
        Refresh,
        Login
    };
public:
    explicit MSAStep(AccountData *data, Action action);
    virtual ~MSAStep() noexcept;

    void perform() override;
    void rehydrate() override;

    QString describe() override;

private slots:
    void onGranted();
    void onRequestFailed(QAbstractOAuth::Error error);
    void onOpenBrowser(const QUrl &url);

private:
    QOAuth2AuthorizationCodeFlow *m_oauth2 = nullptr;
    QOAuthHttpServerReplyHandler *m_replyHandler = nullptr;
    Action m_action;
};
