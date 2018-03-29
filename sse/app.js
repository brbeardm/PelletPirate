
const config = require('./config');
const path = require('path');

var admin = require("firebase-admin");

// Note that you must generate a service account .json file and store it in the directory with config.json. You
// also must specify the filename in FIREBASE_CERT_FILE in config.json.
var serviceAccount = require(path.join(__dirname, config.get('FIREBASE_CERT_FILE')));

// Make sure you set FIREBASE_DATABASE in config.json. Note: It's different than the Google cloud project name!
admin.initializeApp({
	credential: admin.credential.cert(serviceAccount),
	databaseURL: "https://" + config.get('FIREBASE_DATABASE') + ".firebaseio.com"
});

var db = admin.database();
var dbRef = db.ref(config.get('FIREBASE_PARENT'));

var Particle = require('particle-api-js');
var particle = new Particle();

// console.log("deviceFilter=" + config.get('DEVICE_FILTER'));
// console.log("authToken=" + config.get('AUTH_TOKEN'));

// "sse-Temps" is the Particle event name to filter on. It's a prefix, so events beginning with
// this name will be stored in the database.
particle.getEventStream({ deviceId:config.get('DEVICE_FILTER'), auth:config.get('AUTH_TOKEN'), name:'sse-Temps' }).then(
		function(stream) {
			stream.on('event', function(event) {
				console.log("event: ", event);
				storeEvent(event);
			});
		},
		function(err) {
			console.log("Failed to getEventStream for Temps: ", err);
		});

function storeEvent(event) {

	var dbChild = dbRef.child(config.get('FIREBASE_CHILD'));
	
	var data = JSON.parse(event.data);

    // You can uncomment some of the other things if you want to store them in the database
    var obj = {
    	coreid: event.coreid,
    	published_at: event.published_at
    };

    // Copy the data in message.data, the Particle event data, as top-level 
    // elements in obj. This breaks the data out into separate columns.
    for (var prop in data) {
        if (data.hasOwnProperty(prop)) {
            obj[prop] = data[prop];
        }
    }
   
    dbChild.push().set(obj);
}


// this is for the app logging into Firebase via the sse-Logger particle event
particle.getEventStream({ deviceId:config.get('DEVICE_FILTER'), auth:config.get('AUTH_TOKEN'), name:'sse-Logger' }).then(
	function(stream) {
		stream.on('event', function(event) {
			console.log("event: ", event);
			storeEvent2(event);
		});
	},
	function(err) {
		console.log("Failed to getEventStream for Logger: ", err);
	});

function storeEvent2(event) {

var dbChild2 = dbRef.child(config.get('FIREBASE_LOGGER'));

var data2 = JSON.parse(event.data);

// You can uncomment some of the other things if you want to store them in the database
var obj2 = {};
	//coreid: event.coreid,
	published_at: event.published_at
//};

// Copy the data in message.data, the Particle event data, as top-level 
// elements in obj. This breaks the data out into separate columns.
for (var prop2 in data2) {
	if (data2.hasOwnProperty(prop2)) {
		obj2[prop2] = data2[prop2];
	}
}

dbChild2.push(obj2); // changed from dbChild2.push().set(obj2);
}