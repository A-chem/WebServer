FROM ubuntu:22.04

WORKDIR /app

COPY . .

RUN apt update && apt install -y make g++

RUN make

CMD ["./webserv"]
