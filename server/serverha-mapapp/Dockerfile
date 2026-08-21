FROM python:3.12-slim
RUN pip install --no-cache-dir fastapi "uvicorn[standard]" shapely requests
WORKDIR /app
COPY app /app/app
COPY static /app/static
EXPOSE 8000
CMD ["uvicorn","app.main:app","--host","0.0.0.0","--port","8000"]
