# Live race progress

The server broadcasts `RACING_SET_PLAYER_RESET_INFO` when an active racer reaches a new checkpoint or completes a lap. It uses the existing fields: current lap, furthest reset plane, player ID, respawn position, and upcoming plane. The upcoming plane is the furthest plane plus one. No progress broadcast is sent after that racer finishes. Set `disable_live_race_progress=1` in `resources/worldconfig.ini` to disable these extra broadcasts; the default is `0`.

For running position, place racers with a finish placement first, in ascending placement order. Then sort unfinished racers by completed lap descending, furthest plane descending, and the time they reached that plane ascending. Progress broadcasts are reliable and ordered, so a client can record their arrival order for racers tied on lap and plane. If both reach-order values are equal, use ascending player ID for a stable result. A client's running position is based on the most recent progress message it has received; finish placement comes from the existing RacingControl replica.

The initial setup and reset/recovery messages use the same fields. A client should update its recorded reach order only when the lap or furthest plane changes, so a repeated reset message does not move a racer behind another at the same progress.
