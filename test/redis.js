//==================================//
//	Variables						//
//==================================//
const port = 5529;
const pid = process.pid;

//==================================//
//	Requires						//
//==================================//

const { promisify } = require( "util" );
const redis = require( "redis" );
const client = redis.createClient( 6379, "game.fbfafa.0001.usw1.cache.amazonaws.com" );
const pub = redis.createClient( 6379, "game.fbfafa.0001.usw1.cache.amazonaws.com" );
const listener = redis.createClient( 6379, "game.fbfafa.0001.usw1.cache.amazonaws.com" );
const getAsync = promisify( client.hgetall ).bind( client );
const setAsync = promisify( client.hmset ).bind( client );

//==================================//
//	Database						//
//==================================//
client.on( "connection", function() { console.log( "Redis Connected" ); } );
client.on( "ready", async function() {
    console.log( "REDIS READY" );

    console.log( "Setting" );
    await setAsync( "testing", { username:"vintral", password:"Jeff" } );
    console.log( "Getting" );
    let val = await getAsync( "testing" );
    console.log( "Value: " + val );
    console.log( val );

    client.get( "universe", function( err, reply ) {
        console.log( "RETRIEVED UNIVERSE" );
        console.log( "Error: " + err );
        console.log( "Reply: " + reply );

        console.log( reply.length );

        let s = "";
        for( let i = 0; i < reply.length; i++ ) 
            s += parseInt( reply[ i ] ) + " | ";
        console.log( reply[ 2 ] );
    } );

    client.on("subscribe", function (channel, count) {
        pub.publish("space_quest", "TEST MESSAGE FROM NODE.JS");
    });
    
    let msg_count = 0;
    client.on("message", function (channel, message) {
        console.log("sub channel " + channel + ": " + message);
        msg_count += 1;
        if (msg_count === 3) {            
            pub.quit();
        }
    });
    
    client.subscribe("space_quest");
    /*client.on( "subscribe" )
    client.subscribe( "space_quest", ( channel, count ) => {

    } );
    pub.publish( "space_quest", "Hello World" );*/
    //client.log( "SENT" );
} );
client.on( "error", ( err ) => { console.log( "Redis Error: " + err ); } );