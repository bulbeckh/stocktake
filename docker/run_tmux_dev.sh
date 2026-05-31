#!/usr/bin/env bash

SESSION="dev"

tmux has-session -t "$SESSION" 2>/dev/null

if [ $? != 0 ]; then
  tmux new-session -d -s "$SESSION" -n core
  tmux send-keys -t "$SESSION:core" "cd /workspaces/stocktake-alt && \
	  source /opt/ros/jazzy/setup.bash && \
	  source install/setup.bash" C-m

  tmux new-session -d -s "$SESSION" -n vslam
  tmux send-keys -t "$SESSION:vslam" "cd /workspaces/stocktake-alt && \
	  source /opt/ros/jazzy/setup.bash && \
	  source install/setup.bash && \
	  source ../stocktake-vslam/install/setup.bash" C-m

  tmux new-session -d -s "$SESSION" -n orch
  tmux send-keys -t "$SESSION:orch" "cd /workspaces/stocktake-alt && \
	  source /opt/ros/jazzy/setup.bash && \
	  source install/setup.bash" C-m

  tmux new-session -d -s "$SESSION" -n explore
  tmux send-keys -t "$SESSION:explore" "cd /workspaces/stocktake-alt && \
	  source /opt/ros/jazzy/setup.bash && \
	  source install/setup.bash" C-m

  tmux new-session -d -s "$SESSION" -n frontend
  tmux send-keys -t "$SESSION:frontend" "cd /workspaces/stocktake-alt && \
	  source /opt/ros/jazzy/setup.bash && \
	  source install/setup.bash" C-m

  tmux new-session -d -s "$SESSION" -n swagger
  tmux send-keys -t "$SESSION:swagger" "cd /workspaces/stocktake-alt && \
	  source /opt/ros/jazzy/setup.bash && \
	  source install/setup.bash" C-m

fi

tmux attach-session -t "$SESSION"
