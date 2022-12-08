inlets = 1;
outlets = 1;

currentValue = [];

function list()
{
    a = arrayfromargs(arguments);
    var length = arguments.length;
    var inputCh = a[0];
    var newValue = a[1];
    currentValue[inputCh] = newValue;
    var max = currentValue.reduce(function(a,b)
    {
        return Math.max(a,b);
    });
    var index = currentValue.indexOf(max);
    outlet(0, index);
}