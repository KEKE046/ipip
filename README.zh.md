# ipip: 实时信号显示器

ipip是一个**高性能**的**实时**信号显示器。

python和matlab仅需要**一行**代码就可以与ipip交互。

## 显示效果

![](ipip.gif)

**Python 代码:**

```python
import requests, json, time, math

url = 'http://127.0.0.1:1132'
sess = requests.Session()

while True:
    tm = time.time()
    sess.post(url, data=json.dumps({
        # The time keyword must be present in the data
        'time': tm,
        'fig1': {
            'sin': math.sin(tm),
            'cos': math.cos(tm),
        },
        'fig2': {
            'tan': math.tan(tm),
        },
        'fig3': {
            # plot heatmap
            'data': [math.sin(tm), math.cos(tm), math.sin(tm * 2), math.cos(tm * 2)]
        }
    }))
    time.sleep(0.01)
```

**MATLAB 代码:**

```matlab
url='http://127.0.0:1132';
while true
    time = now * 60 * 60 * 24;
    data = struct(...
        % The time keyword must be present in the data
        'time', time,...
        'fig1', struct(...
            'sin', sin(time),...
            'cos', cos(time)...
        )...,
        'fig2', struct('tan', tan(time)),...
        % plot heatmap
        'fig3', struct('data', [sin(time) cos(time) sin(time*2) cos(time*2)]),...
    );
    webwrite(url, data);
    pause(0.01);
end
```

*提示*：JSON的null类型是可以识别的。你可以给某个图的数据赋值为null，IPIP会将其忽略。对于一些时而需要输出，时而不需要输出的数据，这个特性非常有用。

## Binaries

你可以在 [Release 页面](https://github.com/KEKE046/ipip/releases) 中找到编译好的可执行文件。

## 运行

```bash
ipip # 默认在 1132 端口运行
ipip 2333 # 指定在 2333 端口运行
```

## 编译

```bash
git clone https://github.com/KEKE046/ipip.git
cd ipip
git submodule init
git submodule update
mkdir build && cd build
cmake --build . --config Release
sudo cmake --install . # windows上，你不需要执行这句命令，应该直接去build/bin里找ipip.exe，
```

## 建议和意见

如果你有什么想要的新功能或者发现了什么新bug，请在[issue](https://github.com/KEKE046/ipip/issues/new)页面里告知我们。
