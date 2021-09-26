# ipip: Realtime Signal Monitor

[中文README](README.zh.md)

ipip is a **high performance**, **real-time** signal monitor.

You can interact with ipip within only **one line of code** in python or matlab.

## Preview

![](ipip.gif)

**Python Code:**

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

**MATLAB Code:**

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

*Hint*: JSON values of type `null` can be recognised by IPIP. ipip will not add data points for values of NULL. This is useful for data that sometimes needs to be output and sometimes does not need to be output.

## Binaries

Please see the Release Page. Just in the rightside of the filelist.

## Run

```bash
ipip #run on port 1132
```

## Build

```bash
git clone https://github.com/KEKE046/ipip.git
cd ipip
git submodule init
git submodule update
mkdir build && cd build
cmake --build . --config Release
sudo cmake --install . # In windows, you needn't execute this command. you can find the executable file in folder build/bin
```

## Issues

If you want any new features or have found any bugs, please put them in the [issue](https://github.com/KEKE046/ipip/issues/new).