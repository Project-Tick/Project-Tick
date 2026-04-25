from datetime import datetime, timezone

from app.config import ForemanScheduleEntry
from app.services.scheduler import cron_matches


def test_cron_matches_daily_schedule():
    schedule = ForemanScheduleEntry(name="daily", cron="0 3 * * *")

    assert cron_matches(schedule, datetime(2026, 4, 25, 3, 0, tzinfo=timezone.utc)) is True
    assert cron_matches(schedule, datetime(2026, 4, 25, 3, 1, tzinfo=timezone.utc)) is False


def test_cron_matches_weekly_sunday_schedule():
    schedule = ForemanScheduleEntry(name="weekly-sun", cron="0 4 * * 0")

    assert cron_matches(schedule, datetime(2026, 4, 26, 4, 0, tzinfo=timezone.utc)) is True
    assert cron_matches(schedule, datetime(2026, 4, 27, 4, 0, tzinfo=timezone.utc)) is False