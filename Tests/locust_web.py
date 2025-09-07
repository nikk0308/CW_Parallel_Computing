from locust import HttpUser, task, between
import random

SEARCH_TERMS = [
    "test", "hello", "a", "dgdfgdfdgdgdfgf"
]

class WebUser(HttpUser):
    host = "http://127.0.0.1:3000"
    wait_time = between(0.1, 0.5)

    @task(1)
    def search(self):
        term = random.choice(SEARCH_TERMS)
        with self.client.post("/api/command", json={"command": f"search {term}"}, name=f"http/search/{term}", catch_response=True) as r:
            if r.status_code != 200:
                r.failure(f"Bad status {r.status_code}")
            elif "error" in r.text.lower():
                r.failure(f"Error in response: {r.text}")
            else:
                r.success()
