#! /usr/bin/lua

local lapp = require "pl.lapp"

local args = lapp[[
    Filter simulation data
        <xindex> (number)                    index of x data
        <yindex> (number)                    index if y data
        <filename> (string)                  filename of data
        -s,--separator (default ",")         data entry separator
        -S,--print-separator (default " ")   data entry separator
        -r,--reduce (default 0)              reduce number of points to this value (if > 0)
        -f,--filter                          filter data (remove redundant points)
        -n,--every-nth (default 1)           only keep every nth point
        --as-string                          don't do any numerical processing on y
        --digital                            interpret data as digital data, use with --threshold
        --threshold (default 0.0)            threshold for digital data
        --xstart (default -1e32)             minimum x datum
        --xend (default 1e32)                maximum x datum
        --xscale (default 1)                 factor for scaling x data
        --yscale (default 1)                 factor for scaling y data
        --y-is-multibit                      map binary data (e.g. 101) to decimal (e.g. 5) (implies --as-string)
        --ymin (default 0)                   minimum value for y map range
        --ymax (default 0)                   maximum value for y map range
        --xprecision (default 1e-3)          decimal digits for x data
        --yprecision (default 1e-3)          decimal digits for y data
        --xshift (default 0)                 shift x values
        --x-relative                         output relative x values
]]

local filename = args.filename
if not filename then
    print("no filename given")
    return
end

function read_raw_data(filename)
    local raw = {}
    local file = io.open(filename)
    if not file then
        error("read_raw_data: could not open filename")
    end
    while true do
        local line = file:read()
        if not line then
            break
        end
        table.insert(raw, line)
    end
    file:close()
    return raw
end

local function quantize(num, level)
    return level * math.floor(num / level + 0.5)
end

function parse_raw_data(raw, nth, yisnumeric)
    local header = raw[1]
    local data = {}
    local gfmt = "[^" .. args.separator .. "]+"
    for i = 2, #raw, nth do
        local line = raw[i]
        local entry = {}
        local j = 1
        for e in string.gmatch(line, gfmt) do
            if j == args.xindex then
                local num = tonumber(e)
                if num then
                    local x = quantize(args.xscale * num, args.xprecision)
                    if not (x < args.xstart or x > args.xend) then
                        entry.x = x
                    end
                end
            elseif j == args.yindex then
                if not yisnumeric then
                    entry.y = e
                else
                    local num = tonumber(e)
                    if num then
                        local y = quantize(args.yscale * num, args.yprecision)
                        if args.digital then
                            if y > args.threshold then
                                y = 1
                            else
                                y = 0
                            end
                        end
                        entry.y = y
                    end
                end
            end
            j = j + 1
        end
        if entry.x and entry.y then
            table.insert(data, entry)
        end
    end
    return header, data
end

function filter_data(data)
    local out = {}

    -- always print the first point
    local x, y = data[1].x, data[1].y
    table.insert(out, { x = x, y = y })

    local lasty = y
    for i = 2, #data - 1 do
        local x, y = data[i].x, data[i].y
        if math.abs(y - lasty) >= args.yprecision then
            table.insert(out, { x = x, y = y })
        end
        lasty = y
    end

    -- always print the last point
    local x, y = data[#data].x, data[#data].y
    table.insert(out, { x = x, y = y })

    data = out
end

function map_range(data, ymintarget, ymaxtarget)
    local ymin = math.huge
    local ymax = -math.huge
    for i = 1, #data do
        local y = data[i].y
        ymin = math.min(ymin, y)
        ymax = math.max(ymax, y)
    end
    for i = 1, #data do
        data[i].y = (data[i].y - ymin) / (ymax - ymin) * (ymaxtarget - ymintarget) + ymintarget
    end
end

function map_multibit(data)
    local numbits = 0
    for i = 1, #data do
        numbits = math.max(numbits, #data[i].y)
    end
    for i = 1, #data do
        local val = 0
        for j = 1, #data[i].y do
            val = val + tonumber(string.sub(data[i].y, j, j)) * 2^(#data[i].y - j)
        end
        data[i].y = val
    end
end

function reduce_data(data, points)
    return data
end

function print_data(data, yisnumeric)
    if yisnumeric then
        local xdecimals = -math.ceil(math.log(args.xprecision, 10))
        local ydecimals = -math.ceil(math.log(args.yprecision, 10))
        local xdigits = xdecimals + 1 + 0
        local fmt = string.format("%%%d.%df%%s%%%d.%df", 0, xdecimals, 0, ydecimals)
        for _, entry in ipairs(data) do
            print(string.format(fmt, entry.x, args.print_separator, entry.y))
        end
    else
        local xdecimals = -math.ceil(math.log(args.xprecision, 10))
        local xdigits = xdecimals + 1 + 0
        local fmt = string.format("%%%d.%df%%s%%s", 0, xdecimals, 0)
        for _, entry in ipairs(data) do
            print(string.format(fmt, entry.x, args.print_separator, entry.y))
        end
    end
end

local yisnumeric = not args.as_string and not args.y_is_multibit

local raw = read_raw_data(filename)
local header, data = parse_raw_data(raw, args.every_nth, yisnumeric)
if args.filter and yisnumeric then
    filter_data(data)
end
if args.reduce > 0 then
    reduce_data(data, args.reduce)
end
if args.y_is_multibit then
    map_multibit(data)
end
if args.ymin ~= args.ymax then
    map_range(data, args.ymin, args.ymax)
end
if args.x_relative then
    local lastx = data[1].x
    for i = 2, #data do
        local tmp = data[i].x
        data[i].x = data[i].x - lastx
        lastx = tmp
    end
end

-- shift x values
for i = 1, #data do
    data[i].x = data[i].x + args.xshift
end

print_data(data)

