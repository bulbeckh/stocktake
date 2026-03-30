## Stocktake Frontend

Web interface for communication with stocktake robot


## Running
First run the server
```bash
cd stocktake_frontend/frontend
npm run dev
```

There is a python stub for testing. This is a simple server/state-machine that responds to the websocket request
and emulates the mapping/route construction. To run:
```bash
cd stocktake_frontend/frontend/mock_server
python3 websocket_server.py
```

