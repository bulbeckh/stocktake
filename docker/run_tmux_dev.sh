#!/usr/bin/env bash

SESSION="dev"
DIR="/workspaces/stocktake-alt"
SETUP="source /opt/ros/jazzy/setup.bash && source install/setup.bash"

if ! tmux has-session -t "$SESSION" 2>/dev/null; then
  tmux new-session -d -s "$SESSION" -n core
  tmux send-keys -t "$SESSION:core" "cd $DIR && $SETUP" C-m

  tmux new-window -t "$SESSION" -n vslam
  tmux send-keys -t "$SESSION:vslam" "cd $DIR && $SETUP && source ../stocktake-vslam/install/setup.bash" C-m

  tmux new-window -t "$SESSION" -n orch
  tmux send-keys -t "$SESSION:orch" "cd $DIR && $SETUP" C-m

  tmux new-window -t "$SESSION" -n explore
  tmux send-keys -t "$SESSION:explore" "cd $DIR && $SETUP" C-m

  tmux new-window -t "$SESSION" -n frontend
  tmux send-keys -t "$SESSION:frontend" "cd $DIR && $SETUP" C-m

  tmux new-window -t "$SESSION" -n swagger
  tmux send-keys -t "$SESSION:swagger" "cd $DIR && $SETUP" C-m
fi

tmux attach-session -t "$SESSION"
