#include "MSAStep.h"

#include <QNetworkRequest>
#include <QDesktopServices>

#include "minecraft/auth/AuthRequest.h"
#include "minecraft/auth/Parsers.h"

#include "Application.h"

MSAStep::MSAStep(AccountData* data, Action action) : AuthStep(data), m_action(action) {
    m_replyHandler = new QOAuthHttpServerReplyHandler(this);
    m_replyHandler->setCallbackText(tr("Login successful! You can close this page and return to MeshMC."));

    m_oauth2 = new QOAuth2AuthorizationCodeFlow(this);
    m_oauth2->setClientIdentifier(APPLICATION->msaClientId());
    m_oauth2->setAuthorizationUrl(QUrl("https://login.microsoftonline.com/consumers/oauth2/v2.0/authorize"));
    m_oauth2->setTokenUrl(QUrl("https://login.microsoftonline.com/consumers/oauth2/v2.0/token"));
    m_oauth2->setScope("XboxLive.signin offline_access");
    m_oauth2->setReplyHandler(m_replyHandler);
    m_oauth2->setNetworkAccessManager(APPLICATION->network().get());

    connect(m_oauth2, &QOAuth2AuthorizationCodeFlow::granted, this, &MSAStep::onGranted);
    connect(m_oauth2, &QOAuth2AuthorizationCodeFlow::requestFailed, this, &MSAStep::onRequestFailed);
    connect(m_oauth2, &QOAuth2AuthorizationCodeFlow::authorizeWithBrowser, this, &MSAStep::onOpenBrowser);
}

MSAStep::~MSAStep() noexcept = default;

QString MSAStep::describe() {
    return tr("Logging in with Microsoft account.");
}


void MSAStep::rehydrate() {
    switch(m_action) {
        case Refresh: {
            // TODO: check the tokens and see if they are old (older than a day)
            return;
        }
        case Login: {
            // NOOP
            return;
        }
    }
}

void MSAStep::perform() {
    switch(m_action) {
        case Refresh: {
            // Load the refresh token from stored account data
            m_oauth2->setRefreshToken(m_data->msaToken.refresh_token);
            m_oauth2->refreshTokens();
            return;
        }
        case Login: {
            *m_data = AccountData();
            if (!m_replyHandler->isListening()) {
                if (!m_replyHandler->listen(QHostAddress::LocalHost)) {
                    emit finished(AccountTaskState::STATE_FAILED_HARD, tr("Failed to start local HTTP server for OAuth2 callback."));
                    return;
                }
            }
            m_oauth2->setModifyParametersFunction(
                [](QAbstractOAuth::Stage stage, QMultiMap<QString, QVariant> *parameters) {
                    if (stage == QAbstractOAuth::Stage::RequestingAuthorization) {
                        parameters->insert("prompt", "select_account");
                    }
                }
            );
            m_oauth2->grant();
            return;
        }
    }
}

void MSAStep::onOpenBrowser(const QUrl &url) {
    emit authorizeWithBrowser(url);
    QDesktopServices::openUrl(url);
}

void MSAStep::onGranted() {
    m_replyHandler->close();

    // Store the tokens in account data
    m_data->msaToken.token = m_oauth2->token();
    m_data->msaToken.refresh_token = m_oauth2->refreshToken();
    m_data->msaToken.issueInstant = QDateTime::currentDateTimeUtc();
    m_data->msaToken.notAfter = m_oauth2->expirationAt();
    if (!m_data->msaToken.notAfter.isValid()) {
        m_data->msaToken.notAfter = m_data->msaToken.issueInstant.addSecs(3600);
    }
    m_data->msaToken.validity = Katabasis::Validity::Certain;

    emit finished(AccountTaskState::STATE_WORKING, tr("Got MSA token."));
}

void MSAStep::onRequestFailed(QAbstractOAuth::Error error) {
    m_replyHandler->close();

    switch(error) {
        case QAbstractOAuth::Error::NetworkError:
            emit finished(AccountTaskState::STATE_OFFLINE, tr("Microsoft authentication failed due to a network error."));
            return;
        case QAbstractOAuth::Error::ServerError:
        case QAbstractOAuth::Error::OAuthTokenNotFoundError:
        case QAbstractOAuth::Error::OAuthTokenSecretNotFoundError:
        case QAbstractOAuth::Error::OAuthCallbackNotVerified:
            emit finished(AccountTaskState::STATE_FAILED_HARD, tr("Microsoft authentication failed."));
            return;
        case QAbstractOAuth::Error::ExpiredError:
            emit finished(AccountTaskState::STATE_FAILED_GONE, tr("Microsoft authentication token expired."));
            return;
        default:
            emit finished(AccountTaskState::STATE_FAILED_HARD, tr("Microsoft authentication failed with an unrecognized error."));
            return;
    }
}
