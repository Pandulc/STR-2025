FROM python:3.13-slim

WORKDIR /app

COPY requirements.txt ./
COPY app.py ./
COPY .env ./

# Instalar dependencias necesarias para OpenCV y Tesseract
RUN apt-get update && \
    apt-get install -y \
        gcc g++ make cmake \
        libglib2.0-0 libsm6 libxext6 libxrender-dev libgl1 \
        tesseract-ocr && \
    rm -rf /var/lib/apt/lists/*

RUN pip install --no-cache-dir -r requirements.txt

EXPOSE 5000

CMD ["python", "app.py"]
