# マクロ定義
GCC = gcc
FLAGS = -W -Wall
OBJS = server client

# 疑似ターゲットであることを明示
.PHONY: all clean

# すべての実行ファイルを作成
all: $(OBJS)

# 実行ファイルを削除
clean:
	rm -f $(OBJS)

# 生成規則
server: server.c
	$(GCC) $(FLAGS) $+ -o $@

client: client.c
	$(GCC) $(FLAGS) $+ -o $@
