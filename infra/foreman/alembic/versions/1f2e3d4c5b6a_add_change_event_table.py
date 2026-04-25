"""add_change_event_table

Revision ID: 1f2e3d4c5b6a
Revises: e06d44b78199
Create Date: 2026-04-25 12:00:00.000000

"""

from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision: str = "1f2e3d4c5b6a"
down_revision: Union[str, None] = "e06d44b78199"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.create_table(
        "changeevent",
        sa.Column("id", sa.UUID(), nullable=False),
        sa.Column("repository", sa.String(length=255), nullable=False),
        sa.Column("actor", sa.String(length=255), nullable=False),
        sa.Column("ref", sa.String(length=255), nullable=False),
        sa.Column("before_sha", sa.String(length=64), nullable=False),
        sa.Column("after_sha", sa.String(length=64), nullable=False),
        sa.Column("changed_paths", sa.JSON(), nullable=False),
        sa.Column("matched_rule_names", sa.JSON(), nullable=False),
        sa.Column("dispatched_workflows", sa.JSON(), nullable=False),
        sa.Column("pipeline_ids", sa.JSON(), nullable=False),
        sa.Column("created_at", sa.DateTime(timezone=True), nullable=False),
        sa.Column("webhook_event_id", sa.UUID(), nullable=True),
        sa.ForeignKeyConstraint(["webhook_event_id"], ["webhookevent.id"]),
        sa.PrimaryKeyConstraint("id"),
    )
    op.create_index(op.f("ix_changeevent_id"), "changeevent", ["id"], unique=False)
    op.create_index(
        op.f("ix_changeevent_repository"),
        "changeevent",
        ["repository"],
        unique=False,
    )
    op.create_index(op.f("ix_changeevent_actor"), "changeevent", ["actor"], unique=False)
    op.create_index(op.f("ix_changeevent_ref"), "changeevent", ["ref"], unique=False)
    op.create_index(
        op.f("ix_changeevent_after_sha"),
        "changeevent",
        ["after_sha"],
        unique=False,
    )
    op.create_index(
        op.f("ix_changeevent_created_at"),
        "changeevent",
        ["created_at"],
        unique=False,
    )
    op.create_index(
        op.f("ix_changeevent_webhook_event_id"),
        "changeevent",
        ["webhook_event_id"],
        unique=False,
    )


def downgrade() -> None:
    op.drop_index(op.f("ix_changeevent_webhook_event_id"), table_name="changeevent")
    op.drop_index(op.f("ix_changeevent_created_at"), table_name="changeevent")
    op.drop_index(op.f("ix_changeevent_after_sha"), table_name="changeevent")
    op.drop_index(op.f("ix_changeevent_ref"), table_name="changeevent")
    op.drop_index(op.f("ix_changeevent_actor"), table_name="changeevent")
    op.drop_index(op.f("ix_changeevent_repository"), table_name="changeevent")
    op.drop_index(op.f("ix_changeevent_id"), table_name="changeevent")
    op.drop_table("changeevent")