from datetime import datetime
from enum import Enum
from typing import Any

from pydantic import BaseModel, field_validator

from app.models import PipelineStatus, PipelineTrigger


class PipelineType(str, Enum):
    BUILD = "build"


class PipelineTriggerRequest(BaseModel):
    app_id: str
    params: dict[str, Any]


class ExternalPipelineRegistrationRequest(BaseModel):
    app_id: str
    params: dict[str, Any]


class PipelineSummary(BaseModel):
    id: str
    app_id: str
    type: PipelineType = PipelineType.BUILD
    status: PipelineStatus
    repo: str | None = None
    triggered_by: PipelineTrigger
    build_id: int | None = None
    created_at: datetime
    started_at: datetime | None = None
    finished_at: datetime | None = None


class PipelineResponse(BaseModel):
    id: str
    app_id: str
    status: PipelineStatus
    repo: str | None = None
    params: dict[str, Any]
    triggered_by: PipelineTrigger
    log_url: str | None = None
    build_id: int | None = None
    total_cost: float | None = None
    created_at: datetime
    started_at: datetime | None = None
    finished_at: datetime | None = None


class ExternalPipelineRegistrationResponse(BaseModel):
    status: str
    pipeline_id: str
    app_id: str
    pipeline_status: PipelineStatus
    callback_url: str
    callback_token: str


class PipelineStatusCallback(BaseModel):
    status: str

    @field_validator("status")
    @classmethod
    def status_must_be_valid(cls, v):
        if v not in ["success", "failure", "cancelled"]:
            raise ValueError("status must be 'success', 'failure', or 'cancelled'")
        return v


class PipelineLogUrlCallback(BaseModel):
    log_url: str
