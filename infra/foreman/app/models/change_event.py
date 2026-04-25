import uuid
from datetime import datetime

from sqlalchemy import JSON, ForeignKey, String, func
from sqlalchemy.orm import Mapped, mapped_column, relationship

from app.models.webhook_event import Base


class ChangeEvent(Base):
    __tablename__ = "changeevent"

    id: Mapped[uuid.UUID] = mapped_column(
        primary_key=True,
        default=uuid.uuid4,
        index=True,
    )
    repository: Mapped[str] = mapped_column(String(255), index=True)
    actor: Mapped[str] = mapped_column(String(255), index=True)
    ref: Mapped[str] = mapped_column(String(255), index=True)
    before_sha: Mapped[str] = mapped_column(String(64), default="")
    after_sha: Mapped[str] = mapped_column(String(64), index=True)
    changed_paths: Mapped[list[str]] = mapped_column(JSON, default=list)
    matched_rule_names: Mapped[list[str]] = mapped_column(JSON, default=list)
    dispatched_workflows: Mapped[list[dict[str, str]]] = mapped_column(JSON, default=list)
    pipeline_ids: Mapped[list[str]] = mapped_column(JSON, default=list)
    created_at: Mapped[datetime] = mapped_column(default=func.now(), index=True)

    webhook_event_id: Mapped[uuid.UUID | None] = mapped_column(
        ForeignKey("webhookevent.id"),
        nullable=True,
        index=True,
    )
    webhook_event = relationship("WebhookEvent", backref="change_events")

    def __repr__(self) -> str:
        return (
            f"<ChangeEvent(id={self.id}, repository='{self.repository}', "
            f"after_sha='{self.after_sha}')>"
        )