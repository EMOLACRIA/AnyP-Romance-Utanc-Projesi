// Imported from Scripts/Sim/date_data.gd. Original dialogue and choice costs.
const FDateLine Lines[3][4] = {
    {
        { TEXT("You made it. I was starting to think you weren't coming."), {
            { TEXT("2 for 1 hotdogs, what are you gonna do."), -1, 1 },
            { TEXT("I alwasys strive to arrive at the last possible second. Keeps things interesting."), 0, 2 },
            { TEXT("I got nervous seeing you from afar. Lost track of time."), 1, 3 },
        } },
        { TEXT("So, what do you actually do for fun?"), {
            { TEXT("Not much, how about you?"), -1, 1 },
            { TEXT("Strafing mid-air."), 0, 2 },
            { TEXT("Living here you have to get good at parkour. So that is what I do most of the time."), 1, 3 },
        } },
        { TEXT("You've checked your phone like five times. Everything okay?"), {
            { TEXT("It's nothing, just another girl I'm dating."), -1, 1 },
            { TEXT("Daily word puzzle. Just wait a sec."), 0, 2 },
            { TEXT("I think some water splashed all over it. I am just checking if it still works."), 1, 3 },
        } },
        { TEXT("Do you have somewhere else you need to be?"), {
            { TEXT("Yeah, the other girls I mentioned."), -1, 1 },
            { TEXT("It's just a doctor's appointment. Calm down."), 0, 2 },
            { TEXT("Absolutely not. Just got a little flustered."), 1, 3 },
        } },
    },
    {
        { TEXT("You're cutting it pretty close. Were you planning on showing up?"), {
            { TEXT("So much for keeping things interesting."), -1, 1 },
            { TEXT("I don't think I quite have the stamina to climb up this fast."), 0, 2 },
            { TEXT("There were some problems on the path, but I still should have left earlier."), 1, 3 },
        } },
        { TEXT("You look exhausted. What have you been doing all day?"), {
            { TEXT("None of your business."), -1, 1 },
            { TEXT("Strafing. Mid-air variety."), 0, 2 },
            { TEXT("Today has been more exhausting than I would have liked it to be but I'm glad I'm here with you."), 1, 3 },
        } },
        { TEXT("Okay, seriously. Who keeps texting you?"), {
            { TEXT("Refer to my last answer."), -1, 1 },
            { TEXT("Probably other guys who also strafe, mid-air mind you, to gain speed."), 0, 2 },
            { TEXT("It's from the vet. My dog is going through some hard times."), 1, 3 },
        } },
        { TEXT("Are you actually enjoying this, or are you already thinking about leaving?"), {
            { TEXT("You are really dense, aren't you?"), -1, 1 },
            { TEXT("Is there an answer that can convey the in-between?"), 0, 2 },
            { TEXT("I am enjoying it fine. I couldn't not enjoy it with a view like this."), 1, 3 },
        } },
    },
    {
        { TEXT("Did you seriously run all the way up here?"), {
            { TEXT("Compared to your speed, some would consider it walking."), -1, 1 },
            { TEXT("Also quiet a bit of strafing in the air. It gains you speed, you know."), 0, 2 },
            { TEXT("I can assure you that all the other methods were impossible, but I apologize nonetheless."), 1, 3 },
        } },
        { TEXT("You've been acting weird since you got here. What's going on?"), {
            { TEXT("It is caused by your presence."), -1, 1 },
            { TEXT("I think some people don't find air-strafing as important as I do."), 0, 2 },
            { TEXT("I have been in over my head today. Please don't let it distract you from the moment."), 1, 3 },
        } },
        { TEXT("What could you possibly mean by that?"), {
            { TEXT("You are not pleasant to be around."), -1, 1 },
            { TEXT("You too, huh?"), 0, 2 },
            { TEXT("I just tried to do something I normally wouldn't have."), 1, 3 },
        } },
        { TEXT("Is there something you're not telling me?"), {
            { TEXT("I think you get my gist. Why would I even bother telling you anything."), -1, 1 },
            { TEXT("I don't respect you anymore."), 0, 2 },
            { TEXT("Yeah... I think I owe you the truth."), 1, 3 },
        } },
    },
};
