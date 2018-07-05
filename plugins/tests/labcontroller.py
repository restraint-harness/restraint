from flask import make_response
from flask import Flask
from flask import Response
from flask import stream_with_context
import requests
from flask import request
import sys
import logging

logging.basicConfig(level=logging.DEBUG,
                    format='\n[ HTTP Server ] %(message)s)\n'
                    )

app = Flask(__name__)

@app.route('/recipes/<recipes_id>/tasks/<task_id>/results/<result_id>/logs/<logfile>', methods=['PUT'])
def put_data(recipes_id, task_id, result_id, logfile):
    print(request.url)
    with open('server_output/' + logfile, 'w+') as f:
        f.write(request.data.decode('utf-8'))

    return 'OK'

# For when we dont know the result yet. AKA dmesg upload
@app.route('/recipes/<recipes_id>/tasks/<task_id>/logs/<logfile>', methods=['PUT'])
def put_without_resultid(recipes_id, task_id, logfile):
    print(request.url)
    with open('server_output/' + logfile, 'w+') as f:
        f.write(request.data.decode('utf-8'))

    return 'OK'


@app.route('/recipes/<recipes_id>/tasks/<task_id>/results/', methods=['POST'])
def post_data(recipes_id, task_id):
    with open('server_output/rstrnt_result.log', 'w+') as f:
        f.write(request.stream.read(4096).decode('utf-8'))
    response = make_response()
    response.headers['Location'] = request.url + '1'

    return response

if __name__ == '__main__':
    """HTTP server used to fake a beaker labcontroller interaction
       More info here https://beaker-project.org/docs/server-api/http.html#recipe-tasks"""
    app.run(port=8000)
