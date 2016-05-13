// if at least this amount of time happens between a null signal
// and a talk signal, they are considered to have started talking.
var MIN_TALK_LENGTH = 300;
// If we get no signal for this amount of time, consider them no
// longer talking.
var TALK_TIMEOUT = 1000;
// Time length used for storing samples (in ms
var BUFFER_LENGTH = 1000 * 60 * 5; // 5 minutes)
// Prior for cutoff (max loudness that we expect)
var CUTOFF_PROIOR = 6;
// Prior for threshold (speaking threshold, based on our tests)
var THRESHOLD_PRIOR = 20;
//
PRIOR_WEIGHT = 0.9;

/*
This class stores samples for a badge and convert them into talking intervals
It uses a moving RMS to determine a threshold. Since multiplication is
easier than a root operation, we calculate MS and not RMS.
*/
function DataAnalyzer(sampleFreq) {
    this.sampleFreq= sampleFreq;
    var samples = []; // array of samples : timestamp, volume
    var smoother = new SmoothArray(); // array object used for caluclating smooth value

    this.purgeSamples = function(timestamp) {
        while (samples.length > 0  &&(timestamp - samples[0].timestamp > BUFFER_LENGTH)) {
            //console.log("Removing from samples:",samples[0]);
            samples.shift();
        }
    }.bind(this);

    this.addChunk = function(chunk) {
        var startTime = chunk.ts;
        var sampleDuration = chunk.sampleDelay;
        var samples = chunk.samples;
        var analyzer = this;
        $.each(samples, function(i, volume) {
            analyzer.addSample(volume, startTime * 1000 + sampleDuration * i, sampleDuration);
        });

    }.bind(this);

    // Adds a sample. Code assumes that the timestamp is monotonically increasing
    // Code will only add samples newer than the last timestamp
    this.addSample = function(vol, timestamp, duration) {
        // is it a new sample?
        if (samples.length > 0) {
            var lastTimestamp = samples[samples.length - 1].timestamp;
            if (timestamp <= lastTimestamp) {
                console.log("Skipping existing sample:",dateToString((timestamp)));
                return false;
            }
        }

        // remove old objects samples array
        this.purgeSamples(timestamp);

        // add sample to array
        samples.push({'vol': vol,
            'timestamp': timestamp,
            'duration':duration});

        /*
        // Calc smoothened volume
        var sv = smoother.push(vol);

        // update RMS with new sample
        rmsThreshold.addSample(sv,timestamp);
        var meanSquare = rmsThreshold.getThreshold();

        // speaking indicator is 1 if smoothened volume is above the threshold
        var isSpeak = rmsThreshold.compareToThreshold(sv);

        // add sample to array
        samples.push({'volume': vol,
                      'sv' : sv,
                      'meanSquare' : meanSquare ,
                      'isSpeak' : isSpeak,
                      'timestamp': timestamp,
                      'duration':duration});
        */
        return true;
    }.bind(this);

    // Calculates cutiff using data from a CUTOFF_BUFFER_LENGTH ms period
    // before the given periodEndTime. Uses the raw data (before clipping)
    this.calcCutoff = function(periodEndTime) {
        if (samples.length == 0) {
            return CUTOFF_PROIOR;
        }

        meanAndStd = meanAndStd(samples,function(sample) {return sample.vol});
        console.log("Cutoff mean and std:",meanAndStd.mean,meanAndStd.std);// cacl adjusted cutoff (using samples and prior)
        return (CUTOFF_PROIOR*PRIOR_WEIGHT) + (meanAndStd.mean + 2*meanAndStd.std)*(1-PRIOR_WEIGHT);
    }

    /*******************************************************
    Speaking inerval generation
     *******************************************************/
    // Returns true if the two samples are "close" enough together
    this.isWithinTalkTimeout = function (curr, next) {
        var diffInMs = next - curr;
        return  diffInMs <= TALK_TIMEOUT;
    }.bind(this);


    // Returns true if talk interval is long enough
    this.isMinTalkLength = function(talkInterval) {
        var talkIntervalInMs = talkInterval.endTime - talkInterval.startTime;
        return talkIntervalInMs >= MIN_TALK_LENGTH;
    };

    // Generates talking intervals using samples collected during the given time period
    this.generateTalkIntervals = function(periodStartTime,periodEndTime) {
        var talkIntervals = []; // startTime, endTime
        function isInPeriod(sample) {
            return sample.timestamp >= periodStartTime && sample.timestamp <= periodEndTime;
        }
        var samplesInPeriod = samples.filter(isInPeriod);
        function isSpeak(sample) {
            return sample.isSpeak == 1;
        }
        var speakSamples = samplesInPeriod.filter(isSpeak);

        i = 0;
        while (i < speakSamples.length) {
            j = i;

            // traverse the array while samples belong to the same talking interval
            while (j < speakSamples.length - 1 && this.isWithinTalkTimeout(speakSamples[j].timestamp, speakSamples[j+1].timestamp))
            {
                j = j + 1
            }

            // new interval ended. Because each sample is duration millisecond long, we add this to the endTime
            var newTalkInterval = {'startTime':speakSamples[i].timestamp, 'endTime':speakSamples[j].timestamp + speakSamples[j].duration};
            if (this.isMinTalkLength(newTalkInterval)) {
                //console.log("Adding: ",newTalkInterval.startTime,".",newTalkInterval.startTime.getMilliseconds()," ",newTalkInterval.endTime,".",newTalkInterval.endTime.getMilliseconds());
                talkIntervals.push(newTalkInterval);
            }

            // set i to hold the next startTime
            i = j + 1
        }
        return {"data":samplesInPeriod, "intervals":talkIntervals};
    }.bind(this);


    this.getSamples = function() {
        return samples;
    };
}

/*
Class for smoothening the volume signal
*/
function SmoothArray() {
    // Number of samples that will be used for smoothing the samples
    var SAMPLES_SMOOTHING = 5;
    var smoothArray = [];
    var pos = 0;

    this.push = function(vol) {
        // adds the sample to the correct location
        smoothArray[pos] = vol;
        pos = (pos +1) % SAMPLES_SMOOTHING;

        // returns smoothened value
        var sum = smoothArray.reduce(function(a, b) { return a + b; });
        var mean = sum / smoothArray.length;
        return mean;
    };

    this.getSmoothArray = function() {
        return smoothArray;
    }
}

function dateToString(d) {
    var s = d.getFullYear().toString()+"-"+(d.getMonth()+1).toString()+"-"+d.getDate().toString();
    s = s + " "+d.getHours().toString()+":"+d.getMinutes().toString()+":"+d.getSeconds()+"."+d.getMilliseconds().toString();
    return s;
}

// Calculates an average
function average(data,getValue){
    var sum = data.reduce(function(sum, cell){
        var value = getValue(cell);
        return sum + value;
    }, 0);

    var avg = sum / data.length;
    return avg;
}

function meanAndStd(data,getValue) {
    // calc mean
    var mean = average(data,getValue);

    // calc std
    var diffs = data.map(function(cell){
        var diff = getValue(cell) - mean;
        return diff;
    });

    var squareDiffs = data.map(function(cell){
        var diff = getValue(cell) - mean;
        var sqr = diff * diff;
        return sqr;
    });

    var avgSquareDiff = average(squareDiffs,function(v) {return v});
    var stdDev = Math.sqrt(avgSquareDiff);

    return {
        mean: mean,
        std: stdDev
    };
}