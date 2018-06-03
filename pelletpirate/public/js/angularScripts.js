
var app = angular.module(['PelletPirate'],["firebase",'toggle-switch']);
    
    app.controller('ProgramController',function($scope, $firebaseArray) {
        //console.log('in ProgramController');
        this.Program = $firebaseArray(ProgramRef);

        this.add = function add() {
            //this.Program.push({"mode": "Off", "target": 0, "trigger": "Time", "triggerValue": 600})
            this.Program.$add({"mode": "Off", "target": 0, "trigger": "Time", "triggerValue": 600});
        };

    });

//angular.module(['Parameters'],["firebase", 'toggle-switch'])
    app.controller('ParametersController',function($scope, $firebaseObject, $interval) {
        //console.log('in ParametersController');
        ctrl = this;
        this.Auth = false;
        this.Active = false;
     
        var syncObject = $firebaseObject(ParametersRef);
        syncObject.$bindTo($scope, "Parameters");

        function CheckActive() {
            if (T1.length > 0 && ((new Date).getTime() - T1[T1.length-1][0]) < 20000  ) {   // flyman edit - changed the < to 20000 rather than pismokers 5000... this needs to be tied into other logic to determine active cook going on!
                ctrl.Active = true;
                //console.log('In CheckActive true condition');
            } else {
                ctrl.Active = false;
                //console.log('In CheckActive false condition: ' + T1.length + ' dt: ' + ((new Date).getTime() - T1[T1.length-1][0]));
            }
        }

        $interval(CheckActive,3000)

    });